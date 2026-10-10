#pragma once

#include "regram/nagram/services/model.h"

#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtNetwork/QNetworkAccessManager>

class QNetworkReply;
class QHttpMultiPart;

namespace Nagram {

enum class ServiceError {
	None,
	Configuration,
	Credential,
	Network,
	Http,
	Redirect,
	TooLarge,
	Response,
};

struct ServiceResult {
	QByteArray body;
	ServiceError error = ServiceError::None;
	int status = 0;
};

[[nodiscard]] QString ServiceErrorText(ServiceError error, int status = 0);

class ServiceRequest final {
public:
	ServiceRequest();
	~ServiceRequest();
	void cancel();
	void json(
		const ServiceDefinition &service,
		const QJsonObject &body,
		Fn<void(ServiceResult)> done);
	void json(
		const ServiceDefinition &service,
		const QJsonDocument &body,
		const QUrlQuery &query,
		Fn<void(ServiceResult)> done);
	void models(
		const ServiceDefinition &service,
		Fn<void(ServiceResult)> done);
	void audio(
		const ServiceDefinition &service,
		QByteArray bytes,
		QString filename,
		Fn<void(ServiceResult)> done);

private:
	[[nodiscard]] std::optional<QNetworkRequest> prepare(
		const ServiceDefinition &service,
		Fn<void(ServiceResult)> &done);
	void start(QNetworkReply *reply, Fn<void(ServiceResult)> done);

	QNetworkAccessManager _network;
	QTimer _deadline;
	QPointer<QNetworkReply> _reply;
	QByteArray _body;
	bool _tooLarge = false;

};

} // namespace Nagram
