#include "regram/nagram/services/request.h"

#include "lang/lang_keys.h"
#include "regram/nagram/services/credentials.h"

#include <QtCore/QJsonDocument>
#include <QtNetwork/QHttpMultiPart>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Nagram {
namespace {

constexpr auto kMaximumResponse = 2 * 1024 * 1024;
constexpr auto kMaximumJson = 128 * 1024;
constexpr auto kMaximumAudio = 24 * 1024 * 1024;
constexpr auto kRequestTimeout = std::chrono::seconds(120);

} // namespace

QString ServiceErrorText(ServiceError error, int status) {
	switch (error) {
	case ServiceError::Configuration:
		return tr::lng_nagram_service_invalid(tr::now);
	case ServiceError::Credential:
		return tr::lng_nagram_service_key_error(tr::now);
	case ServiceError::Network:
		return tr::lng_nagram_service_network_error(tr::now);
	case ServiceError::Http:
		return tr::lng_nagram_service_http_error(tr::now, lt_code, QString::number(status));
	case ServiceError::Redirect:
		return tr::lng_nagram_service_redirect_error(tr::now);
	case ServiceError::TooLarge:
		return tr::lng_nagram_service_size_error(tr::now);
	case ServiceError::Response:
		return tr::lng_nagram_service_response_error(tr::now);
	case ServiceError::None:
		return QString();
	}
	Unexpected("Invalid service error.");
}

ServiceRequest::ServiceRequest() {
	_deadline.setSingleShot(true);
	QObject::connect(&_deadline, &QTimer::timeout, &_network, [=] {
		if (_reply) {
			_reply->abort();
		}
	});
}

ServiceRequest::~ServiceRequest() {
	cancel();
}

void ServiceRequest::cancel() {
	_deadline.stop();
	if (const auto reply = _reply.data()) {
		_reply.clear();
		QObject::disconnect(reply, nullptr, &_network, nullptr);
		reply->abort();
		reply->deleteLater();
	}
	_body.clear();
}

std::optional<QNetworkRequest> ServiceRequest::prepare(
		const ServiceDefinition &service,
		Fn<void(ServiceResult)> &done) {
	cancel();
	if (!ParseService(SerializeService(service))) {
		done({ .error = ServiceError::Configuration });
		return std::nullopt;
	}
	auto request = QNetworkRequest(ServiceEndpoint(service));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::ManualRedirectPolicy);
	request.setTransferTimeout(kRequestTimeout);
	if (service.useKey) {
		auto credential = ReadCredential(CredentialAccount(service));
		if (credential.error != CredentialError::None || credential.secret.isEmpty()) {
			done({ .error = ServiceError::Credential });
			return std::nullopt;
		}
		for (auto &[name, value] : AuthHeaders(service, credential.secret)) {
			request.setRawHeader(name, value);
			value.fill('\0');
		}
		credential.secret.fill('\0');
	}
	return request;
}

void ServiceRequest::json(
		const ServiceDefinition &service,
		const QJsonObject &body,
		Fn<void(ServiceResult)> done) {
	json(service, QJsonDocument(body), QUrlQuery(), std::move(done));
}

void ServiceRequest::json(
		const ServiceDefinition &service,
		const QJsonDocument &body,
		const QUrlQuery &query,
		Fn<void(ServiceResult)> done) {
	const auto bytes = body.toJson(QJsonDocument::Compact);
	if (bytes.size() > kMaximumJson) {
		cancel();
		done({ .error = ServiceError::TooLarge });
		return;
	}
	auto request = prepare(service, done);
	if (!request) {
		return;
	}
	if (!query.isEmpty()) {
		auto url = request->url();
		url.setQuery(query);
		request->setUrl(url);
	}
	request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	start(_network.post(*request, bytes), std::move(done));
}

void ServiceRequest::models(
		const ServiceDefinition &service,
		Fn<void(ServiceResult)> done) {
	if (!LlmProtocol(service.protocol)) {
		cancel();
		done({ .error = ServiceError::Configuration });
		return;
	}
	auto request = prepare(service, done);
	if (!request) {
		return;
	}
	auto catalog = service;
	catalog.endpoint = u"models"_q;
	request->setUrl(ServiceEndpoint(catalog));
	start(_network.get(*request), std::move(done));
}

void ServiceRequest::audio(
		const ServiceDefinition &service,
		QByteArray bytes,
		QString filename,
		Fn<void(ServiceResult)> done) {
	if (bytes.isEmpty() || bytes.size() > kMaximumAudio) {
		cancel();
		done({ .error = ServiceError::TooLarge });
		return;
	}
	static const auto extensions = QStringList{
		u"flac"_q, u"mp3"_q, u"mp4"_q, u"mpeg"_q, u"mpga"_q,
		u"m4a"_q, u"ogg"_q, u"wav"_q, u"webm"_q,
	};
	const auto extension = filename.section('.', -1).toLower();
	if (service.kind != ServiceKind::Transcription
		|| service.protocol != u"openai"_q || !extensions.contains(extension)) {
		cancel();
		done({ .error = ServiceError::Configuration });
		return;
	}
	auto request = prepare(service, done);
	if (!request) {
		return;
	}
	const auto multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
	const auto add = [&](const QByteArray &key, const QString &value) {
		auto part = QHttpPart();
		part.setHeader(QNetworkRequest::ContentDispositionHeader,
			"form-data; name=\"" + key + "\"");
		part.setBody(value.toUtf8());
		multipart->append(part);
	};
	add("model", service.model);
	add("response_format", u"json"_q);
	if (!service.prompt.isEmpty()) {
		add("prompt", service.prompt);
	}
	if (!service.language.isEmpty()) {
		add("language", service.language);
	}
	if (service.temperature) {
		add("temperature", QString::number(*service.temperature));
	}
	auto file = QHttpPart();
	file.setHeader(QNetworkRequest::ContentDispositionHeader,
		u"form-data; name=\"file\"; filename=\"audio.%1\""_q.arg(extension));
	file.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
	file.setBody(bytes);
	multipart->append(file);
	const auto reply = _network.post(*request, multipart);
	multipart->setParent(reply);
	start(reply, std::move(done));
}

void ServiceRequest::start(QNetworkReply *reply, Fn<void(ServiceResult)> done) {
	_reply = reply;
	_deadline.start(kRequestTimeout);
	_body.clear();
	_tooLarge = false;
	reply->setReadBufferSize(kMaximumResponse + 1);
	const auto read = [=] {
		_body += reply->read(kMaximumResponse + 1 - _body.size());
		if (_body.size() > kMaximumResponse) {
			_tooLarge = true;
			reply->abort();
		}
	};
	QObject::connect(reply, &QNetworkReply::readyRead, &_network, read);
	QObject::connect(reply, &QNetworkReply::finished, &_network,
		[=, done = std::move(done)]() mutable {
			_deadline.stop();
			const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			if (!_tooLarge) {
				read();
			}
			const auto error = _tooLarge ? ServiceError::TooLarge
				: (status >= 300 && status < 400) ? ServiceError::Redirect
				: (status && (status < 200 || status >= 300)) ? ServiceError::Http
				: (reply->error() != QNetworkReply::NoError || !status) ? ServiceError::Network
				: ServiceError::None;
			auto result = ServiceResult{
				.body = error == ServiceError::None ? std::move(_body) : QByteArray(),
				.error = error,
				.status = status,
			};
			_reply.clear();
			reply->deleteLater();
			done(std::move(result));
		});
}

} // namespace Nagram
