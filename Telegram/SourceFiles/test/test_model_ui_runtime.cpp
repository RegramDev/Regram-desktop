#include <crl/crl_on_main.h>
#include <rpl/never.h>

namespace crl {

rpl::producer<> on_main_update_requests() {
	return rpl::never<>();
}

} // namespace crl
