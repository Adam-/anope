/*
 * (C) 2003-2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "modules/httpd.h"

namespace WebCPanel
{

class ResetPass : public WebPanelPage
{
 public:
	ResetPass(const Anope::string &u) : WebPanelPage(u) { }

	bool OnRequest(HTTPProvider *, const Anope::string &, HTTPClient *, HTTPMessage &, HTTPReply &) anope_override;
};

}

