/* hs_request.c - Add request and activate functionality to HostServ,
 *				along with adding +req as optional param to HostServ list.
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Based on the original module by Rob <rob@anope.org>
 * Included in the Anope module pack since Anope 1.7.11
 * Anope Coder: GeniusDex <geniusdex@anope.org>
 *
 * Please read COPYING and README for further details.
 *
 * Send bug reports to the Anope Coder instead of the module
 * author, because any changes since the inclusion into anope
 * are not supported by the original author.
 */

#include "module.h"

#define AUTHOR "Rob"

/* Configuration variables */
int HSRequestMemoUser = 0;
int HSRequestMemoOper = 0;
int HSRequestMemoSetters = 0;

/* Language defines */
enum
{
	LNG_REQUEST_SYNTAX,
	LNG_REQUESTED,
	LNG_REQUEST_WAIT,
	LNG_REQUEST_MEMO,
	LNG_ACTIVATE_SYNTAX,
	LNG_ACTIVATED,
	LNG_ACTIVATE_MEMO,
	LNG_REJECT_SYNTAX,
	LNG_REJECTED,
	LNG_REJECT_MEMO,
	LNG_REJECT_MEMO_REASON,
	LNG_NO_REQUEST,
	LNG_HELP,
	LNG_HELP_SETTER,
	LNG_HELP_REQUEST,
	LNG_HELP_ACTIVATE,
	LNG_HELP_ACTIVATE_MEMO,
	LNG_HELP_REJECT,
	LNG_HELP_REJECT_MEMO,
	LNG_WAITING_SYNTAX,
	LNG_HELP_WAITING,
	LNG_NUM_STRINGS
};

void my_add_host_request(const Anope::string &nick, const Anope::string &vIdent, const Anope::string &vhost, const Anope::string &creator, time_t tmp_time);
int my_isvalidchar(char c);
void my_memo_lang(User *u, const Anope::string &name, int z, int number, ...);
void req_send_memos(User *u, const Anope::string &vIdent, const Anope::string &vHost);

void my_load_config();
void my_add_languages();

struct HostRequest
{
	Anope::string ident;
	Anope::string host;
	time_t time;
};

typedef std::map<Anope::string, HostRequest *, std::less<ci::string> > RequestMap;
RequestMap Requests;

static Module *me;

class CommandHSRequest : public Command
{
 public:
	CommandHSRequest() : Command("REQUEST", 1, 1)
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		Anope::string nick = u->nick;
		Anope::string rawhostmask = params[0];
		Anope::string hostmask;
		NickAlias *na;
		time_t now = time(NULL);

		Anope::string vIdent = myStrGetToken(rawhostmask, '@', 0); /* Get the first substring, @ as delimiter */
		if (!vIdent.empty())
		{
			rawhostmask = myStrGetTokenRemainder(rawhostmask, '@', 1); /* get the remaining string */
			if (rawhostmask.empty())
			{
				me->NoticeLang(Config->s_HostServ, u, LNG_REQUEST_SYNTAX);
				return MOD_CONT;
			}
			if (vIdent.length() > Config->UserLen)
			{
				notice_lang(Config->s_HostServ, u, HOST_SET_IDENTTOOLONG, Config->UserLen);
				return MOD_CONT;
			}
			else
				for (Anope::string::iterator s = vIdent.begin(), s_end = vIdent.end(); s != s_end; ++s)
					if (!my_isvalidchar(*s))
					{
						notice_lang(Config->s_HostServ, u, HOST_SET_IDENT_ERROR);
						return MOD_CONT;
					}
			if (!ircd->vident)
			{
				notice_lang(Config->s_HostServ, u, HOST_NO_VIDENT);
				return MOD_CONT;
			}
		}
		if (rawhostmask.length() < Config->HostLen)
			hostmask = rawhostmask;
		else
		{
			notice_lang(Config->s_HostServ, u, HOST_SET_TOOLONG, Config->HostLen);
			return MOD_CONT;
		}

		if (!isValidHost(hostmask, 3))
		{
			notice_lang(Config->s_HostServ, u, HOST_SET_ERROR);
			return MOD_CONT;
		}

		if ((na = findnick(nick)))
		{
			if ((HSRequestMemoOper || HSRequestMemoSetters) && Config->MSSendDelay > 0 && u && u->lastmemosend + Config->MSSendDelay > now)
			{
				me->NoticeLang(Config->s_HostServ, u, LNG_REQUEST_WAIT, Config->MSSendDelay);
				u->lastmemosend = now;
				return MOD_CONT;
			}
			my_add_host_request(nick, vIdent, hostmask, u->nick, now);

			me->NoticeLang(Config->s_HostServ, u, LNG_REQUESTED);
			req_send_memos(u, vIdent, hostmask);
			Log(LOG_COMMAND, u, this, NULL) << "to request new vhost " << (!vIdent.empty() ? vIdent + "@" : "") << hostmask;
		}
		else
			notice_lang(Config->s_HostServ, u, HOST_NOREG, nick.c_str());

		return MOD_CONT;
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_REQUEST_SYNTAX);
		u->SendMessage(Config->s_HostServ, " ");
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP_REQUEST);

		return true;
	}

	void OnSyntaxError(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_REQUEST_SYNTAX);
	}

	void OnServHelp(User *u)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP);
	}
};

class CommandHSActivate : public Command
{
 public:
	CommandHSActivate() : Command("ACTIVATE", 1, 1, "hostserv/set")
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		Anope::string nick = params[0];
		NickAlias *na;

		if ((na = findnick(nick)))
		{
			RequestMap::iterator it = Requests.find(na->nick);
			if (it != Requests.end())
			{
				na->hostinfo.SetVhost(it->second->ident, it->second->host, u->nick, it->second->time);

				if (HSRequestMemoUser)
					my_memo_lang(u, na->nick, 2, LNG_ACTIVATE_MEMO);

				me->NoticeLang(Config->s_HostServ, u, LNG_ACTIVATED, na->nick.c_str());
				Log(LOG_COMMAND, u, this, NULL) << "for " << na->nick << " for vhost " << (!it->second->ident.empty() ? it->second->ident + "@" : "") << it->second->host;
				delete it->second;
				Requests.erase(it);
			}
			else
				me->NoticeLang(Config->s_HostServ, u, LNG_NO_REQUEST, nick.c_str());
		}
		else
			notice_lang(Config->s_HostServ, u, NICK_X_NOT_REGISTERED, nick.c_str());

		return MOD_CONT;
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_ACTIVATE_SYNTAX);
		u->SendMessage(Config->s_HostServ, " ");
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP_ACTIVATE);
		if (HSRequestMemoUser)
			me->NoticeLang(Config->s_HostServ, u, LNG_HELP_ACTIVATE_MEMO);

		return true;
	}

	void OnSyntaxError(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_ACTIVATE_SYNTAX);
	}

	void OnServHelp(User *u)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP_SETTER);
	}
};

class CommandHSReject : public Command
{
 public:
	CommandHSReject() : Command("REJECT", 1, 2, "hostserv/set")
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		Anope::string nick = params[0];
		Anope::string reason = params.size() > 1 ? params[1] : "";

		RequestMap::iterator it = Requests.find(nick);
		if (it != Requests.end())
		{
			delete it->second;
			Requests.erase(it);

			if (HSRequestMemoUser)
			{
				if (!reason.empty())
					my_memo_lang(u, nick, 2, LNG_REJECT_MEMO_REASON, reason.c_str());
				else
					my_memo_lang(u, nick, 2, LNG_REJECT_MEMO);
			}

			me->NoticeLang(Config->s_HostServ, u, LNG_REJECTED, nick.c_str());
			Log(LOG_COMMAND, u, this, NULL) << "to reject vhost for " << nick << " (" << (!reason.empty() ? reason : "") << ")";
		}
		else
			me->NoticeLang(Config->s_HostServ, u, LNG_NO_REQUEST, nick.c_str());

		return MOD_CONT;
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_REJECT_SYNTAX);
		u->SendMessage(Config->s_HostServ, " ");
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP_REJECT);
		if (HSRequestMemoUser)
			me->NoticeLang(Config->s_HostServ, u, LNG_HELP_REJECT_MEMO);

		return true;
	}

	void OnSyntaxError(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_REJECT_SYNTAX);
	}
};

class HSListBase : public Command
{
 protected:
	CommandReturn DoList(User *u)
	{
		char buf[BUFSIZE];
		int counter = 1;
		int from = 0, to = 0;
		unsigned display_counter = 0;
		tm *tm;

		for (RequestMap::iterator it = Requests.begin(), it_end = Requests.end(); it != it_end; ++it)
		{
			HostRequest *hr = it->second;
			if (((counter >= from && counter <= to) || (!from && !to)) && display_counter < Config->NSListMax)
			{
				++display_counter;
				tm = localtime(&hr->time);
				strftime(buf, sizeof(buf), getstring(u, STRFTIME_DATE_TIME_FORMAT), tm);
				if (!hr->ident.empty())
					notice_lang(Config->s_HostServ, u, HOST_IDENT_ENTRY, counter, it->first.c_str(), hr->ident.c_str(), hr->host.c_str(), it->first.c_str(), buf);
				else
					notice_lang(Config->s_HostServ, u, HOST_ENTRY, counter, it->first.c_str(), hr->host.c_str(), it->first.c_str(), buf);
			}
			++counter;
		}
		notice_lang(Config->s_HostServ, u, HOST_LIST_FOOTER, display_counter);

		return MOD_CONT;
	}
 public:
	HSListBase(const Anope::string &cmd, int min, int max) : Command(cmd, min, max, "hostserv/set")
	{
	}

	void OnSyntaxError(User *u, const Anope::string &subcommand)
	{
		// no-op
	}
};

class CommandHSWaiting : public HSListBase
{
 public:
	CommandHSWaiting() : HSListBase("WAITING", 0, 0)
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		return this->DoList(u);
	}

	bool OnHelp(User *u, const Anope::string &subcommand)
	{
		me->NoticeLang(Config->s_HostServ, u, LNG_WAITING_SYNTAX);
		u->SendMessage(Config->s_HostServ, " ");
		me->NoticeLang(Config->s_HostServ, u, LNG_HELP_WAITING);

		return true;
	}
};

class HSRequest : public Module
{
	CommandHSRequest commandhsrequest;
	CommandHSActivate commandhsactive;
	CommandHSReject commandhsreject;
	CommandHSWaiting commandhswaiting;

 public:
	HSRequest(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		me = this;

		this->AddCommand(HostServ, &commandhsrequest);
		this->AddCommand(HostServ, &commandhsactive);
		this->AddCommand(HostServ, &commandhsreject);
		this->AddCommand(HostServ, &commandhswaiting);

		this->SetAuthor(AUTHOR);
		this->SetType(SUPPORTED);

		my_load_config();

		const char *langtable_en_us[] = {
			/* LNG_REQUEST_SYNTAX */
			"Syntax: \002REQUEST \037vhost\037\002",
			/* LNG_REQUESTED */
			"Your vHost has been requested",
			/* LNG_REQUEST_WAIT */
			"Please wait %d seconds before requesting a new vHost",
			/* LNG_REQUEST_MEMO */
			"[auto memo] vHost \002%s\002 has been requested.",
			/* LNG_ACTIVATE_SYNTAX */
			"Syntax: \002ACTIVATE \037nick\037\002",
			/* LNG_ACTIVATED */
			"vHost for %s has been activated",
			/* LNG_ACTIVATE_MEMO */
			"[auto memo] Your requested vHost has been approved.",
			/* LNG_REJECT_SYNTAX */
			"Syntax: \002REJECT \037nick\037\002",
			/* LNG_REJECTED */
			"vHost for %s has been rejected",
			/* LNG_REJECT_MEMO */
			"[auto memo] Your requested vHost has been rejected.",
			/* LNG_REJECT_MEMO_REASON */
			"[auto memo] Your requested vHost has been rejected. Reason: %s",
			/* LNG_NO_REQUEST */
			"No request for nick %s found.",
			/* LNG_HELP */
			"    REQUEST     Request a vHost for your nick",
			/* LNG_HELP_SETTER */
			"    ACTIVATE    Approve the requested vHost of a user\n"
			"    REJECT      Reject the requested vHost of a user\n"
			"    WAITING     Convenience command for LIST +req",
			/* LNG_HELP_REQUEST */
			"Request the given vHost to be actived for your nick by the\n"
			"network administrators. Please be patient while your request\n"
			"is being considered.",
			/* LNG_HELP_ACTIVATE */
			"Activate the requested vHost for the given nick.",
			/* LNG_HELP_ACTIVATE_MEMO */
			"A memo informing the user will also be sent.",
			/* LNG_HELP_REJECT */
			"Reject the requested vHost for the given nick.",
			/* LNG_HELP_REJECT_MEMO */
			"A memo informing the user will also be sent.",
			/* LNG_WAITING_SYNTAX */
			"Syntax: \002WAITING\002",
			/* LNG_HELP_WAITING */
			"This command is provided for convenience. It is essentially\n"
			"the same as performing a LIST +req ."
		};

		const char *langtable_nl[] = {
			/* LNG_REQUEST_SYNTAX */
			"Gebruik: \002REQUEST \037vhost\037\002",
			/* LNG_REQUESTED */
			"Je vHost is aangevraagd",
			/* LNG_REQUEST_WAIT */
			"Wacht %d seconden voor je een nieuwe vHost aanvraagt",
			/* LNG_REQUEST_MEMO */
			"[auto memo] vHost \002%s\002 is aangevraagd.",
			/* LNG_ACTIVATE_SYNTAX */
			"Gebruik: \002ACTIVATE \037nick\037\002",
			/* LNG_ACTIVATED */
			"vHost voor %s is geactiveerd",
			/* LNG_ACTIVATE_MEMO */
			"[auto memo] Je aangevraagde vHost is geaccepteerd.",
			/* LNG_REJECT_SYNTAX */
			"Gebruik: \002REJECT \037nick\037\002",
			/* LNG_REJECTED */
			"vHost voor %s is afgekeurd",
			/* LNG_REJECT_MEMO */
			"[auto memo] Je aangevraagde vHost is afgekeurd.",
			/* LNG_REJECT_MEMO_REASON */
			"[auto memo] Je aangevraagde vHost is afgekeurd. Reden: %s",
			/* LNG_NO_REQUEST */
			"Geen aanvraag voor nick %s gevonden.",
			/* LNG_HELP */
			"    REQUEST     Vraag een vHost aan voor je nick",
			/* LNG_HELP_SETTER */
			"    ACTIVATE    Activeer de aangevraagde vHost voor een gebruiker\n"
			"    REJECT      Keur de aangevraagde vHost voor een gebruiker af\n"
			"    WAITING     Snelkoppeling naar LIST +req",
			/* LNG_HELP_REQUEST */
			"Verzoek de gegeven vHost te activeren voor jouw nick bij de\n"
			"netwerk beheerders. Het kan even duren voordat je aanvraag\n"
			"afgehandeld wordt.",
			/* LNG_HELP_ACTIVATE */
			"Activeer de aangevraagde vHost voor de gegeven nick.",
			/* LNG_HELP_ACTIVATE_MEMO */
			"Een memo die de gebruiker op de hoogste stelt zal ook worden verstuurd.",
			/* LNG_HELP_REJECT */
			"Keur de aangevraagde vHost voor de gegeven nick af.",
			/* LNG_HELP_REJECT_MEMO */
			"Een memo die de gebruiker op de hoogste stelt zal ook worden verstuurd.",
			/* LNG_WAITING_SYNTAX */
			"Gebruik: \002WAITING\002",
			/* LNG_HELP_WAITING */
			"Dit commando is beschikbaar als handigheid. Het is simpelweg\n"
			"hetzelfde als LIST +req ."
		};

		const char *langtable_pt[] = {
			/* LNG_REQUEST_SYNTAX */
			"Sintaxe: \002REQUEST \037vhost\037\002",
			/* LNG_REQUESTED */
			"Seu pedido de vHost foi encaminhado",
			/* LNG_REQUEST_WAIT */
			"Por favor, espere %d segundos antes de fazer um novo pedido de vHost",
			/* LNG_REQUEST_MEMO */
			"[Auto Memo] O vHost \002%s\002 foi solicitado.",
			/* LNG_ACTIVATE_SYNTAX */
			"Sintaxe: \002ACTIVATE \037nick\037\002",
			/* LNG_ACTIVATED */
			"O vHost para %s foi ativado",
			/* LNG_ACTIVATE_MEMO */
			"[Auto Memo] Seu pedido de vHost foi aprovado.",
			/* LNG_REJECT_SYNTAX */
			"Sintaxe: \002REJECT \037nick\037\002",
			/* LNG_REJECTED */
			"O vHost de %s foi recusado",
			/* LNG_REJECT_MEMO */
			"[Auto Memo] Seu pedido de vHost foi recusado.",
			/* LNG_REJECT_MEMO_REASON */
			"[Auto Memo] Seu pedido de vHost foi recusado. Motivo: %s",
			/* LNG_NO_REQUEST */
			"Nenhum pedido encontrado para o nick %s.",
			/* LNG_HELP */
			"    REQUEST     Request a vHost for your nick",
			/* LNG_HELP_SETTER */
			"    ACTIVATE    Aprova o pedido de vHost de um usu�rio\n"
			"    REJECT      Recusa o pedido de vHost de um usu�rio\n"
			"    WAITING     Comando para LISTAR +req",
			/* LNG_HELP_REQUEST */
			"Solicita a ativa��o do vHost fornecido em seu nick pelos\n"
			"administradores da rede. Por favor, tenha paci�ncia\n"
			"enquanto seu pedido � analisado.",
			/* LNG_HELP_ACTIVATE */
			"Ativa o vHost solicitado para o nick fornecido.",
			/* LNG_HELP_ACTIVATE_MEMO */
			"Um memo informando o usu�rio tamb�m ser� enviado.",
			/* LNG_HELP_REJECT */
			"Recusa o pedido de vHost para o nick fornecido.",
			/* LNG_HELP_REJECT_MEMO */
			"Um memo informando o usu�rio tamb�m ser� enviado.",
			/* LNG_WAITING_SYNTAX */
			"Sintaxe: \002WAITING\002",
			/* LNG_HELP_WAITING */
			"Este comando � usado por conveni�ncia. � essencialmente\n"
			"o mesmo que fazer um LIST +req"
		};

		const char *langtable_ru[] = {
			/* LNG_REQUEST_SYNTAX */
			"���������: \002REQUEST \037vHost\037\002",
			/* LNG_REQUESTED */
			"��� ������ �� vHost ���������.",
			/* LNG_REQUEST_WAIT */
			"����������, ��������� %d ������, ������ ��� ����������� ����� vHost",
			/* LNG_REQUEST_MEMO */
			"[����-���������] ��� �������� vHost \002%s\002",
			/* LNG_ACTIVATE_SYNTAX */
			"���������: \002ACTIVATE \037���\037\002",
			/* LNG_ACTIVATED */
			"vHost ��� %s ������� �����������",
			/* LNG_ACTIVATE_MEMO */
			"[����-���������] ������������� ���� vHost ��������� � �����������.",
			/* LNG_REJECT_SYNTAX */
			"���������: \002REJECT \037���\037\002",
			/* LNG_REJECTED */
			"vHost ��� %s ��������.",
			/* LNG_REJECT_MEMO */
			"[����-���������] ������������� ���� vHost ��������.",
			/* LNG_REJECT_MEMO_REASON */
			"[����-���������] ������������� ���� vHost ��������. �������: %s",
			/* LNG_NO_REQUEST */
			"������ �� vHost ��� ���� %s �� ������.",
			/* LNG_HELP */
			"    REQUEST     ������ �� vHost ��� ������ �������� ����",
			/* LNG_HELP_SETTER */
			"    ACTIVATE    ��������� ������������� �������������  vHost\n"
			"    REJECT      ��������� ������������� �������������  vHost\n"
			"    WAITING     ������ �������� ��������� ��������� (������ LIST +req)",
			/* LNG_HELP_REQUEST */
			"���������� ������ �� ��������� vHost, ������� ����� ���������� ����� ��\n"
			"��������������� ����. ������� �������� ��������, ���� ������\n"
			"��������������� ��������������.",
			/* LNG_HELP_ACTIVATE */
			"��������� ������������� vHost ��� ���������� ����.",
			/* LNG_HELP_ACTIVATE_MEMO */
			"������������ ����� ������� ����-����������� �� ��������� ��� �������.",
			/* LNG_HELP_REJECT */
			"��������� ������������� vHost ��� ���������� ����.",
			/* LNG_HELP_REJECT_MEMO */
			"������������ ����� ������� ����-����������� �� ���������� ��� �������.",
			/* LNG_WAITING_SYNTAX */
			"���������: \002WAITING\002",
			/* LNG_HELP_WAITING */
			"������ ������� ������� ��� �������� ������������� � ������� ������ ��������,\n"
			"��������� ���������. ����������� �������: LIST +req ."
		};

		const char *langtable_it[] = {
			/* LNG_REQUEST_SYNTAX */
			"Sintassi: \002REQUEST \037vhost\037\002",
			/* LNG_REQUESTED */
			"Il tuo vHost � stato richiesto",
			/* LNG_REQUEST_WAIT */
			"Prego attendere %d secondi prima di richiedere un nuovo vHost",
			/* LNG_REQUEST_MEMO */
			"[auto memo] � stato richiesto il vHost \002%s\002.",
			/* LNG_ACTIVATE_SYNTAX */
			"Sintassi: \002ACTIVATE \037nick\037\002",
			/* LNG_ACTIVATED */
			"Il vHost per %s � stato attivato",
			/* LNG_ACTIVATE_MEMO */
			"[auto memo] Il vHost da te richiesto � stato approvato.",
			/* LNG_REJECT_SYNTAX */
			"Sintassi: \002REJECT \037nick\037\002",
			/* LNG_REJECTED */
			"Il vHost per %s � stato rifiutato",
			/* LNG_REJECT_MEMO */
			"[auto memo] Il vHost da te richiesto � stato rifiutato.",
			/* LNG_REJECT_MEMO_REASON */
			"[auto memo] Il vHost da te richiesto � stato rifiutato. Motivo: %s",
			/* LNG_NO_REQUEST */
			"Nessuna richiesta trovata per il nick %s.",
			/* LNG_HELP */
			"    REQUEST     Richiede un vHost per il tuo nick",
			/* LNG_HELP_SETTER */
			"    ACTIVATE    Approva il vHost richiesto di un utente\n"
			"    REJECT      Rifiuta il vHost richiesto di un utente\n"
			"    WAITING     Comando per LIST +req",
			/* LNG_HELP_REQUEST */
			"Richiede l'attivazione del vHost specificato per il tuo nick da parte\n"
			"degli amministratori di rete. Sei pregato di pazientare finch� la tua\n"
			"richiesta viene elaborata.",
			/* LNG_HELP_ACTIVATE */
			"Attiva il vHost richiesto per il nick specificato.",
			/* LNG_HELP_ACTIVATE_MEMO */
			"Viene inviato un memo per informare l'utente.",
			/* LNG_HELP_REJECT */
			"Rifiuta il vHost richiesto per il nick specificato.",
			/* LNG_HELP_REJECT_MEMO */
			"Viene inviato un memo per informare l'utente.",
			/* LNG_WAITING_SYNTAX */
			"Sintassi: \002WAITING\002",
			/* LNG_HELP_WAITING */
			"Questo comando � per comodit�. Praticamente � la stessa cosa che\n"
			"eseguire un LIST +req ."
		};

		this->InsertLanguage(LANG_EN_US, LNG_NUM_STRINGS, langtable_en_us);
		this->InsertLanguage(LANG_NL, LNG_NUM_STRINGS, langtable_nl);
		this->InsertLanguage(LANG_PT, LNG_NUM_STRINGS, langtable_pt);
		this->InsertLanguage(LANG_RU, LNG_NUM_STRINGS, langtable_ru);
		this->InsertLanguage(LANG_IT, LNG_NUM_STRINGS, langtable_it);

		Implementation i[] = { I_OnPreCommand, I_OnDatabaseRead, I_OnDatabaseWrite };
		ModuleManager::Attach(i, this, 3);
	}

	~HSRequest()
	{
		/* Clean up all open host requests */
		while (!Requests.empty())
		{
			delete Requests.begin()->second;
			Requests.erase(Requests.begin());
		}
	}

	EventReturn OnPreCommand(User *u, BotInfo *service, const Anope::string &command, const std::vector<Anope::string> &params)
	{
		if (!Config->s_HostServ.empty() && service == findbot(Config->s_HostServ))
		{
			if (command.equals_ci("LIST"))
			{
				Anope::string key = params.size() ? params[0] : "";

				if (!key.empty() && key.equals_ci("+req"))
				{
					std::vector<Anope::string> emptyParams;
					Command *c = FindCommand(HostServ, "WAITING");
					c->Execute(u, emptyParams);
					return EVENT_STOP;
				}
			}
		}
		else if (service == findbot(Config->s_NickServ))
		{
			if (command.equals_ci("DROP"))
			{
				NickAlias *na = findnick(u->nick);

				if (na)
				{
					RequestMap::iterator it = Requests.find(na->nick);

					if (it != Requests.end())
					{
						delete it->second;
						Requests.erase(it);
					}
				}
			}
		}

		return EVENT_CONTINUE;
	}

	EventReturn OnDatabaseRead(const std::vector<Anope::string> &params)
	{
		if (params[0].equals_ci("HS_REQUEST") && params.size() >= 5)
		{
			Anope::string vident = params[2].equals_ci("(null)") ? "" : params[2];
			my_add_host_request(params[1], vident, params[3], params[1], params[4].is_pos_number_only() ? convertTo<time_t>(params[4]) : 0);

			return EVENT_STOP;
		}

		return EVENT_CONTINUE;
	}

	void OnDatabaseWrite(void (*Write)(const Anope::string &))
	{
		for (RequestMap::iterator it = Requests.begin(), it_end = Requests.end(); it != it_end; ++it)
		{
			HostRequest *hr = it->second;
			std::stringstream buf;
			buf << "HS_REQUEST " << it->first << " " << (hr->ident.empty() ? "(null)" : hr->ident) << " " << hr->host << " " << hr->time;
			Write(buf.str());
		}
	}
};

void my_memo_lang(User *u, const Anope::string &name, int z, int number, ...)
{
	va_list va;
	char buffer[4096], outbuf[4096];
	char *fmt = NULL;
	int lang = LANG_EN_US;
	char *s, *t, *buf;
	User *u2;

	u2 = finduser(name);
	/* Find the users lang, and use it if we cant */
	if (u2 && u2->Account())
		lang = u2->Account()->language;

	/* If the users lang isnt supported, drop back to enlgish */
	if (!me->lang[lang].argc)
		lang = LANG_EN_US;

	/* If the requested lang string exists for the language */
	if (me->lang[lang].argc > number)
	{
		fmt = me->lang[lang].argv[number];

		buf = strdup(fmt); // XXX
		s = buf;
		while (*s)
		{
			t = s;
			s += strcspn(s, "\n");
			if (*s)
				*s++ = '\0';
			strscpy(outbuf, t, sizeof(outbuf));

			va_start(va, number);
			vsnprintf(buffer, 4095, outbuf, va);
			va_end(va);
			memo_send(u, name, buffer, z);
		}
		free(buf); // XXX
	}
	else
		Log() << me->name << ": INVALID language string call, language: [" << lang << "], String [" << number << "]";
}

void req_send_memos(User *u, const Anope::string &vIdent, const Anope::string &vHost)
{
	Anope::string host;
	std::list<std::pair<Anope::string, Anope::string> >::iterator it, it_end;

	if (!vIdent.empty())
		host = vIdent + "@" + vHost;
	else
		host = vHost;

	if (HSRequestMemoOper == 1)
		for (it = Config->Opers.begin(), it_end = Config->Opers.end(); it != it_end; ++it)
		{
			Anope::string nick = it->first;
			my_memo_lang(u, nick, 2, LNG_REQUEST_MEMO, host.c_str());
		}
	if (HSRequestMemoSetters == 1)
	{
		/* Needs to be rethought because of removal of HostSetters in favor of opertype priv -- CyberBotX
		for (i = 0; i < HostNumber; ++i)
			my_memo_lang(u, HostSetters[i], z, LNG_REQUEST_MEMO, host);*/
	}
}

void my_add_host_request(const Anope::string &nick, const Anope::string &vIdent, const Anope::string &vhost, const Anope::string &creator, time_t tmp_time)
{
	HostRequest *hr = new HostRequest;
	hr->ident = vIdent;
	hr->host = vhost;
	hr->time = tmp_time;
	RequestMap::iterator it = Requests.find(nick);
	if (it != Requests.end())
	{
		delete it->second;
		Requests.erase(it);
	}
	Requests.insert(std::make_pair(nick, hr));
}

int my_isvalidchar(char c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-')
		return 1;
	else
		return 0;
}

void my_load_config()
{
	ConfigReader config;
	HSRequestMemoUser = config.ReadFlag("hs_request", "memouser", "no", 0);
	HSRequestMemoOper = config.ReadFlag("hs_request", "memooper", "no", 0);
	HSRequestMemoSetters = config.ReadFlag("hs_request", "memosetters", "no", 0);

	Log(LOG_DEBUG) << "[hs_request] Set config vars: MemoUser=" << HSRequestMemoUser << " MemoOper=" <<  HSRequestMemoOper << " MemoSetters=" << HSRequestMemoSetters;
}

MODULE_INIT(HSRequest)
