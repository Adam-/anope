/*
 * (C) 2003-2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "../webcpanel.h"

bool WebCPanel::ResetPass::OnRequest(HTTPProvider *server, const Anope::string &page_name, HTTPClient *client, HTTPMessage &message, HTTPReply &reply)
{
	TemplateFileServer::Replacements replacements;
	replacements["TITLE"] = page_title;

	const Anope::string &nick = message.post_data["nickname"], &email = message.post_data["email"];
	if (!nick.empty() && !email.empty())
	{
		std::vector<Anope::string> params;
		params.push_back(nick);
		params.push_back(email);

		WebPanel::RunCommand(user, NULL, "NickServ", "nickserv/register", params, replacements);
	}

	TemplateFileServer page("resetpass.html");

	page.Serve(server, page_name, client, message, reply, replacements);
	return true;
}

