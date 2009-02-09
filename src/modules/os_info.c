/* os_info.c - Adds oper information lines to nicks/channels
 *
 * (C) 2003-2009 Anope Team
 * Contact us at team@anope.org
 *
 * Based on the original module by Rob <rob@anope.org>
 * Included in the Anope module pack since Anope 1.7.9
 * Anope Coder: DrStein <drstein@anope.org>
 *
 * Please read COPYING and README for further details.
 *
 * Send bug reports to the Anope Coder instead of the module
 * author, because any changes since the inclusion into anope
 * are not supported by the original author.
 *
 */
/*************************************************************************/

#include "module.h"

#define AUTHOR "Rob"
#define VERSION "$Id$"

/* Default database name */
#define DEFAULT_DB_NAME "os_info.db"

/* Multi-language stuff */
#define LANG_NUM_STRINGS   10

#define OINFO_SYNTAX		0
#define OINFO_ADD_SUCCESS   1
#define OINFO_DEL_SUCCESS   2
#define OCINFO_SYNTAX	   3
#define OCINFO_ADD_SUCCESS  4
#define OCINFO_DEL_SUCCESS  5
#define OINFO_HELP		  6
#define OCINFO_HELP		 7
#define OINFO_HELP_CMD	  8
#define OCINFO_HELP_CMD	 9

/*************************************************************************/

char *OSInfoDBName = NULL;

void mMainChanHelp(User *u);
void mMainNickHelp(User *u);

int mLoadData();
int mSaveData(int argc, char **argv);
int mBackupData(int argc, char **argv);
int mLoadConfig();
int mEventReload(int argc, char **argv);

static Module *me;

/*************************************************************************/

class CommandNSOInfo : public Command
{
 private:
	CommandResult DoAdd(User *u, std::vector<std::string> &params)
	{
		const char *nick = params[1].c_str();
		const char *info = params.size() > 2 ? params[2].c_str() : NULL;
		NickAlias *na = NULL;

		if (!info)
		{
			this->OnSyntaxError(u);
			return MOD_CONT;
		}

		if ((na = findnick(nick))) /* ok we've found the user */
		{
			/* Add the module data to the user */
			na->nc->Extend("os_info", sstrdup(info));
			me->NoticeLang(s_NickServ, u, OINFO_ADD_SUCCESS, nick);

		}
		else /* NickCore not found! */
			notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED, nick);

		return MOD_CONT;
	}

	CommandResult DoDel(User *u, std::vector<std::string> &params)
	{
		const char *nick = params[1].c_str();
		NickAlias *na = NULL;

		if ((na = findnick(nick))) /* ok we've found the user */
		{
			char *c;
			if (na->nc->GetExt("os_info", c))
			{
				delete [] c;
				na->nc->Shrink("os_info");
			}

			me->NoticeLang(s_NickServ, u, OINFO_DEL_SUCCESS, nick);

		}
		else /* NickCore not found! */
			notice_lang(s_NickServ, u, NICK_X_NOT_REGISTERED, nick);

		return MOD_CONT;
	}
 public:
	CommandNSOInfo() : Command("OINFO", 2, 3)
	{
	}

	CommandResult Execute(User *u, std::vector<std::string> &params)
	{
		const char *cmd = params[0].c_str();

		if (!strcasecmp(cmd, "ADD"))
			return this->DoAdd(u, params);
		else if (!strcasecmp(cmd, "DEL"))
			return this->DoDel(u, params);
		else
			this->OnSyntaxError(u);
		return MOD_CONT;
	}

	bool OnHelp(User *u, const std::string &subcommand)
	{
		if (!is_oper(u))
			return false;

		me->NoticeLang(s_NickServ, u, OINFO_HELP);
		return true;
	}

	void OnSyntaxError(User *u)
	{
		me->NoticeLang(s_NickServ, u, OINFO_SYNTAX);
	}
};

class CommandNSInfo : public Command
{
 public:
	CommandNSInfo() : Command("INFO", 1, 1)
	{
	}

	CommandResult Execute(User *u, std::vector<std::string> &params)
	{
		const char *nick = params[0].c_str();
		NickAlias *na = NULL;

		if (is_oper(u)) /* Only show our goodies to opers */
		{
			if ((na = findnick(nick))) /* ok we've found the user */
			{
				/* If we have any info on this user */
				char *c;
				if (na->nc->GetExt("os_info", c))
				{
					notice_user(s_NickServ, u, " OperInfo: %s", c);
				}
			}
		}
		return MOD_CONT;
	}

	void OnSyntaxError(User *u)
	{
		// no-op
	}
};

class CommandCSOInfo : public Command
{
 private:
	CommandResult DoAdd(User *u, std::vector<std::string> &params)
	{
		const char *chan = params[1].c_str();
		const char *info = params.size() > 2 ? params[2].c_str() : NULL;
		ChannelInfo *ci = NULL;

		if (!info)
		{
			this->OnSyntaxError(u);
			return MOD_CONT;
		}

		if ((ci = cs_findchan(chan)))
		{
			/* Add the module data to the channel */
			ci->Extend("os_info", sstrdup(info));
			me->NoticeLang(s_ChanServ, u, OCINFO_ADD_SUCCESS, chan);
		}
		else
			notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, chan);

		return MOD_CONT;
	}

	CommandResult DoDel(User *u, std::vector<std::string> &params)
	{
		const char *chan = params[1].c_str();
		ChannelInfo *ci = NULL;

		if ((ci = cs_findchan(chan)))
		{
			/* Del the module data from the channel */
			char *c;
			if (ci->GetExt("os_info", c))
			{
				delete [] c;
				ci->Shrink("os_info");
			}
			me->NoticeLang(s_ChanServ, u, OCINFO_DEL_SUCCESS, chan);
		}
		else
			notice_lang(s_ChanServ, u, CHAN_X_NOT_REGISTERED, chan);

		return MOD_CONT;
	}
 public:
	CommandCSOInfo : Command("OINFO", 2, 3)
	{
	}

	CommandResult Execute(User *u, std::vector<std::string> &params)
	{
		const char *cmd = params[0].c_str();

		if (!strcasecmp(cmd, "ADD"))
			return this->DoAdd(u, params);
		else if (!strcasecmp(cmd, "DEL"))
			return this->DoDel(u, params);
		else
			this->OnSyntaxError(u);
		return MOD_CONT;
	}

	bool OnHelp(User *u, const std::string &subcommand)
	{
		if (!is_oper(u))
			return false;

		me->NoticeLang(s_ChanServ, u, OCINFO_HELP);
		return true;
	}

	void OnSyntaxError(User *u)
	{
		me->NoticeLang(s_ChanServ, u, OCINFO_SYNTAX);
	}
};

class CommandCSInfo : public Command
{
 public:
	CommandCSInfo() : Command("INFO", 1, 1)
	{
	}

	CommandResult Execute(User *u, std::vector<std::string> &params)
	{
		char *chan = params[0].c_str();
		ChannelInfo *ci = NULL;

		if (is_oper(u)) /* Only show our goodies to opers */
		{
			if ((ci = cs_findchan(chan)))
			{
				/* If we have any info on this channel */
				char *c;
				if (ci->GetExt("os_info", c))
					notice_user(s_ChanServ, u, " OperInfo: %s", c);
			}
		}
		return MOD_CONT;
	}

	void OnSyntaxError(User *u)
	{
		// no-op
	}
};

class OSInfo : public Module
{
 public:
	OSInfo(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		EvtHook *hook = NULL;

		int status;

		me = this;

		this->SetAuthor(AUTHOR);
		this->SetVersion(VERSION);
		this->SetType(SUPPORTED);

		if (mLoadConfig())
			throw ModuleException("Unable to load config");

		status = this->AddCommand(NICKSERV, new CommandNSOInfo(), MOD_HEAD);
		status = this->AddCommand(NICKSERV, new CommandNSInfo(), MOD_TAIL);

		status = this->AddCommand(CHANSERV, new CommandCSOInfo(), MOD_HEAD);
		status = this->AddCommand(CHANSERV, new CommandCSInfo(), MOD_TAIL);

		hook = createEventHook(EVENT_DB_SAVING, mSaveData);
		status = this->AddEventHook(hook);

		hook = createEventHook(EVENT_DB_BACKUP, mBackupData);
		status = this->AddEventHook(hook);

		hook = createEventHook(EVENT_RELOAD, mEventReload);
		status = this->AddEventHook(hook);

		this->SetNickHelp(mMainNickHelp);
		this->SetChanHelp(mMainChanHelp);

		mLoadData();

		const char* langtable_en_us[] = {
			/* OINFO_SYNTAX */
			"Syntax: OINFO [ADD|DEL] nick <info>",
			/* OINFO_ADD_SUCCESS */
			"OperInfo line has been added to nick %s",
			/* OINFO_DEL_SUCCESS */
			"OperInfo line has been removed from nick %s",
			/* OCINFO_SYNTAX */
			"Syntax: OINFO [ADD|DEL] chan <info>",
			/* OCINFO_ADD_SUCCESS */
			"OperInfo line has been added to channel %s",
			/* OCINFO_DEL_SUCCESS */
			"OperInfo line has been removed from channel %s",
			/* OINFO_HELP */
			"Syntax: OINFO [ADD|DEL] nick <info>\n"
			"Add or Delete Oper information for the given nick\n"
			"This will show up when any oper /ns info nick's the user.\n"
			"and can be used for 'tagging' users etc....",
			/* OCINFO_HELP */
			"Syntax: OINFO [ADD|DEL] chan <info>\n"
			"Add or Delete Oper information for the given channel\n"
			"This will show up when any oper /cs info's the channel.\n"
			"and can be used for 'tagging' channels etc....",
			/* OINFO_HELP_CMD */
			"    OINFO      Add / Del an OperInfo line to a nick",
			/* OCINFO_HELP_CMD */
			"    OINFO      Add / Del an OperInfo line to a channel"
		};

		const char* langtable_es[] = {
			/* OINFO_SYNTAX */
			"Sintaxis: OINFO [ADD|DEL] nick <info>",
			/* OINFO_ADD_SUCCESS */
			"Una linea OperInfo ha sido agregada al nick %s",
			/* OINFO_DEL_SUCCESS */
			"La linea OperInfo ha sido removida del nick %s",
			/* OCINFO_SYNTAX */
			"Sintaxis: OINFO [ADD|DEL] chan <info>",
			/* OCINFO_ADD_SUCCESS */
			"Linea OperInfo ha sido agregada al canal %s",
			/* OCINFO_DEL_SUCCESS */
			"La linea OperInfo ha sido removida del canal %s",
			/* OINFO_HELP */
			"Sintaxis: OINFO [ADD|DEL] nick <info>\n"
			"Agrega o elimina informacion para Operadores al nick dado\n"
			"Esto se mostrara cuando cualquier operador haga /ns info nick\n"
			"y puede ser usado para 'marcado' de usuarios, etc....",
			/* OCINFO_HELP */
			"Sintaxis: OINFO [ADD|DEL] chan <info>\n"
			"Agrega o elimina informacion para Operadores al canal dado\n"
			"Esto se mostrara cuando cualquier operador haga /cs info canal\n"
			"y puede ser usado para 'marcado' de canales, etc....",
			/* OINFO_HELP_CMD */
			"    OINFO      Agrega / Elimina una linea OperInfo al nick",
			/* OCINFO_HELP_CMD */
			"    OINFO      Agrega / Elimina una linea OperInfo al canal"
		};

		const char* langtable_nl[] = {
			/* OINFO_SYNTAX */
			"Gebruik: OINFO [ADD|DEL] nick <info>",
			/* OINFO_ADD_SUCCESS */
			"OperInfo regel is toegevoegd aan nick %s",
			/* OINFO_DEL_SUCCESS */
			"OperInfo regel is weggehaald van nick %s",
			/* OCINFO_SYNTAX */
			"Gebruik: OINFO [ADD|DEL] kanaal <info>",
			/* OCINFO_ADD_SUCCESS */
			"OperInfo regel is toegevoegd aan kanaal %s",
			/* OCINFO_DEL_SUCCESS */
			"OperInfo regel is weggehaald van kanaal %s",
			/* OINFO_HELP */
			"Gebruik: OINFO [ADD|DEL] nick <info>\n"
			"Voeg een Oper informatie regel toe aan de gegeven nick, of\n"
			"verwijder deze. Deze regel zal worden weergegeven wanneer\n"
			"een oper /ns info nick doet voor deze gebruiker, en kan worden\n"
			"gebruikt om een gebruiker te 'markeren' etc...",
			/* OCINFO_HELP */
			"Gebruik: OINFO [ADD|DEL] kanaal <info>\n"
			"Voeg een Oper informatie regel toe aan de gegeven kanaal, of\n"
			"verwijder deze. Deze regel zal worden weergegeven wanneer\n"
			"een oper /cs info kanaal doet voor dit kanaal, en kan worden\n"
			"gebruikt om een kanaal te 'markeren' etc...",
			/* OINFO_HELP_CMD */
			"    OINFO      Voeg een OperInfo regel toe aan een nick of verwijder deze",
			/* OCINFO_HELP_CMD */
			"    OINFO         Voeg een OperInfo regel toe aan een kanaal of verwijder deze"
		};

		const char* langtable_de[] = {
			/* OINFO_SYNTAX */
			"Syntax: OINFO [ADD|DEL] Nickname <Information>",
			/* OINFO_ADD_SUCCESS */
			"Eine OperInfo Linie wurde zu den Nicknamen %s hinzugef�gt",
			/* OINFO_DEL_SUCCESS */
			"Die OperInfo Linie wurde von den Nicknamen %s enfernt",
			/* OCINFO_SYNTAX */
			"Syntax: OINFO [ADD|DEL] Channel <Information>",
			/* OCINFO_ADD_SUCCESS */
			"Eine OperInfo Linie wurde zu den Channel %s hinzugef�gt",
			/* OCINFO_DEL_SUCCESS */
			"Die OperInfo Linie wurde von den Channel %s enfernt",
			/* OINFO_HELP */
			"Syntax: OINFO [ADD|DEL] Nickname <Information>\n"
			"Addiert oder l�scht eine OperInfo Linie zu den angegebenen\n"
			"Nicknamen.Sie wird angezeigt wenn ein Oper mit /ns info sich\n"
			"�ber den Nicknamen informiert.",
			/* OCINFO_HELP */
			"Syntax: OINFO [ADD|DEL] chan <info>\n"
			"Addiert oder l�scht eine OperInfo Linie zu den angegebenen\n"
			"Channel.Sie wird angezeigt wenn ein Oper mit /cs info sich\n"
			"�ber den Channel informiert.",
			/* OINFO_HELP_CMD */
			"    OINFO      Addiert / L�scht eine OperInfo Linie zu / von einen Nicknamen",
			/* OCINFO_HELP_CMD */
			"    OINFO      Addiert / L�scht eine OperInfo Linie zu / von einen Channel"
		};

		const char* langtable_pt[] = {
			/* OINFO_SYNTAX */
			"Sintaxe: OINFO [ADD|DEL] nick <informa��o>",
			/* OINFO_ADD_SUCCESS */
			"A linha OperInfo foi adicionada ao nick %s",
			/* OINFO_DEL_SUCCESS */
			"A linha OperInfo foi removida do nick %s",
			/* OCINFO_SYNTAX */
			"Sintaxe: OINFO [ADD|DEL] canal <informa��o>",
			/* OCINFO_ADD_SUCCESS */
			"A linha OperInfo foi adicionada ao canal %s",
			/* OCINFO_DEL_SUCCESS */
			"A linha OperInfo foi removida do canal %s",
			/* OINFO_HELP */
			"Sintaxe: OINFO [ADD|DEL] nick <informa��o>\n"
			"Adiciona ou apaga informa��o para Operadores ao nick fornecido\n"
			"Isto ser� mostrado quando qualquer Operador usar /ns info nick\n"
			"e pode ser usado para 'etiquetar' usu�rios etc...",
			/* OCINFO_HELP */
			"Sintaxe: OINFO [ADD|DEL] canal <informa��o>\n"
			"Adiciona ou apaga informa��o para Operadores ao canal fornecido\n"
			"Isto ser� mostrado quando qualquer Operador usar /cs info canal\n"
			"e pode ser usado para 'etiquetar' canais etc...",
			/* OINFO_HELP_CMD */
			"    OINFO      Adiciona ou Apaga a linha OperInfo para um nick",
			/* OCINFO_HELP_CMD */
			"    OINFO      Adiciona ou Apaga a linha OperInfo para um canal"
		};

		const char* langtable_ru[] = {
			/* OINFO_SYNTAX */
			"���������: OINFO ADD|DEL ��� ����",
			/* OINFO_ADD_SUCCESS */
			"����-���������� ��� ���� %s ���������",
			/* OINFO_DEL_SUCCESS */
			"����-���������� ��� ���� %s ���� �������",
			/* OCINFO_SYNTAX */
			"���������: OINFO ADD|DEL #����� �����",
			/* OCINFO_ADD_SUCCESS */
			"����-���������� ��� ������ %s ������� �����������",
			/* OCINFO_DEL_SUCCESS */
			"����-���������� ��� ������ %s ���� �������",
			/* OINFO_HELP */
			"���������: OINFO ADD|DEL ��� �����\n"
			"������������� ��� ������� ����-���������� ��� ���������� ����,\n"
			"������� ����� �������� ������ ���������, �������������� INFO ����.\n"
			"����� ���� ������������ ��� '�������' ������������� � �. �...",
			/* OCINFO_HELP */
			"���������: OINFO ADD|DEL #����� �����\n"
			"������������� ��� ������� ����-���������� ��� ���������� ������,\n"
			"������� ����� �������� ������ ���������, �������������� INFO ������.\n"
			"����� ���� ������������ ��� '�������' ������� � �. �...",
			/* OINFO_HELP_CMD */
			"    OINFO      ���������/������� ����-���� ��� ����",
			/* OCINFO_HELP_CMD */
			"    OINFO      ���������/������� ����-���� ��� ������"
		};

		const char* langtable_it[] = {
			/* OINFO_SYNTAX */
			"Sintassi: OINFO [ADD|DEL] nick <info>",
			/* OINFO_ADD_SUCCESS */
			"Linea OperInfo aggiunta al nick %s",
			/* OINFO_DEL_SUCCESS */
			"Linea OperInfo rimossa dal nick %s",
			/* OCINFO_SYNTAX */
			"Sintassi: OINFO [ADD|DEL] chan <info>",
			/* OCINFO_ADD_SUCCESS */
			"Linea OperInfo aggiunta al canale %s",
			/* OCINFO_DEL_SUCCESS */
			"Linea OperInfo rimossa dal canale %s",
			/* OINFO_HELP */
			"Sintassi: OINFO [ADD|DEL] nick <info>\n"
			"Aggiunge o rimuove informazioni Oper per il nick specificato\n"
			"Queste vengono mostrate quando un oper esegue il comando /ns info <nick>\n"
			"e possono essere utilizzate per 'marchiare' gli utenti ecc...",
			/* OCINFO_HELP */
			"Sintassi: OINFO [ADD|DEL] chan <info>\n"
			"Aggiunge o rimuove informazioni Oper per il canale specificato\n"
			"Queste vengono mostrate quando un oper esegue il comando /cs info <canale>\n"
			"e possono essere utilizzate per 'marchiare' i canali ecc...",
			/* OINFO_HELP_CMD */
			"    OINFO      Aggiunge/Rimuove una linea OperInfo ad/da un nick",
			/* OCINFO_HELP_CMD */
			"    OINFO      Aggiunge/Rimuove una linea OperInfo ad/da un canale"
		};

		this->InsertLanguage(LANG_EN_US, LANG_NUM_STRINGS, langtable_en_us);
		this->InsertLanguage(LANG_ES, LANG_NUM_STRINGS, langtable_es);
		this->InsertLanguage(LANG_NL, LANG_NUM_STRINGS, langtable_nl);
		this->InsertLanguage(LANG_DE, LANG_NUM_STRINGS, langtable_de);
		this->InsertLanguage(LANG_PT, LANG_NUM_STRINGS, langtable_pt);
		this->InsertLanguage(LANG_RU, LANG_NUM_STRINGS, langtable_ru);
		this->InsertLanguage(LANG_IT, LANG_NUM_STRINGS, langtable_it);
	}

	~OSInfo()
	{
		char *av[1];

		for (int i = 0; i < 1024; ++i)
		{
			/* Remove the nick Cores */
			for (NickCore *nc = nclists[i]; nc; nc = nc->next)
			{
				char *c;
				if (nc->GetExt("os_modinfo", c));
				{
					delete [] c;
					nc->Shrink("os_modinfo");
				}
			}
		}
		av[0] = sstrdup(EVENT_START);
		mSaveData(1, av);
		delete [] av[0];

		if (OSInfoDBName)
			delete [] OSInfoDBName;
	}
};

/*************************************************************************/

/**
 * Load data from the db file, and populate our OperInfo lines
 * @return 0 for success
 **/
int mLoadData()
{
	int ret = 0;
	FILE *in;

	char *type = NULL;
	char *name = NULL;
	char *info = NULL;
	int len = 0;

	ChannelInfo *ci = NULL;
	NickAlias *na = NULL;

	/* will _never_ be this big thanks to the 512 limit of a message */
	char buffer[2000];
	if (!(in = fopen(OSInfoDBName, "r")))
	{
		alog("os_info: WARNING: can not open the database file! (it might not exist, this is not fatal)");
		ret = 1;
	}
	else
	{
		while (fgets(buffer, 1500, in))
		{
			type = myStrGetToken(buffer, ' ', 0);
			name = myStrGetToken(buffer, ' ', 1);
			info = myStrGetTokenRemainder(buffer, ' ', 2);
			if (type)
			{
				if (name)
				{
					if (info)
					{
						len = strlen(info);
						/* Take the \n from the end of the line */
						info[len - 1] = '\0';
						if (!stricmp(type, "C"))
						{
							if ((ci = cs_findchan(name)))
								ci->Extend("os_info", strdup("info"));
						}
						else if (!stricmp(type, "N"))
						{
							if ((na = findnick(name)))
								na->nc->Extend("os_info", strdup(info));
						}
						delete [] info;
					}
					delete [] name;
				}
				delete [] type;
			}
		}
	}
	return ret;
}

/**
 * Save all our data to our db file
 * First walk through the nick CORE list, and any nick core which has
 * oper info attached to it, write to the file.
 * Next do the same again for ChannelInfos
 * @return 0 for success
 **/
int mSaveData(int argc, char **argv)
{
	ChannelInfo *ci = NULL;
	NickCore *nc = NULL;
	int i = 0;
	int ret = 0;
	FILE *out;

	if (argc >= 1)
	{
		if (!stricmp(argv[0], EVENT_START))
		{
			if (!(out = fopen(OSInfoDBName, "w")))
			{
				alog("os_info: ERROR: can not open the database file!");
				ircdproto->SendGlobops(s_OperServ, "os_info: ERROR: can not open the database file!");
				ret = 1;
			}
			else
			{
				for (i = 0; i < 1024; ++i)
				{
					for (nc = nclists[i]; nc; nc = nc->next)
					{
						/* If we have any info on this user */
						char *c;
						if (nc->GetExt("os_info", c))
							fprintf(out, "N %s %s\n", nc->display, c);
					}
				}

				for (i = 0; i < 256; ++i)
				{
					for (ci = chanlists[i]; ci; ci = ci->next)
					{
						/* If we have any info on this channel */
						char *c;
						if (ci->GetExt("os_info", c))
							fprintf(out, "C %s %s\n", ci->name, c);
					}
				}
				fclose(out);
			}
		}
		else
			ret = 0;
	}

	return ret;
}

/**
 * Backup our databases using the commands provided by Anope
 * @return MOD_CONT
 **/
int mBackupData(int argc, char **argv)
{
	ModuleDatabaseBackup(OSInfoDBName);

	return MOD_CONT;
}

/**
 * Load the configuration directives from Services configuration file.
 * @return 0 for success
 **/
int mLoadConfig()
{
	ConfigReader config;
	std::string tmp = config.ReadValue("os_info", "database", DEFAULT_DB_NAME, 0);

	if (OSInfoDBName)
		delete [] OSInfoDBName;

	OSInfoDBName = sstrdup(tmp.c_str());

	alog("os_info: Directive OSInfoDBName loaded (%s)...", OSInfoDBName);

	return 0;
}

/**
 * Manage the RELOAD EVENT
 * @return MOD_CONT
 **/
int mEventReload(int argc, char **argv)
{
	int ret = 0;
	if (argc >= 1)
	{
		if (!stricmp(argv[0], EVENT_START))
		{
			alog("os_info: Reloading configuration directives...");
			ret = mLoadConfig();
		}
	}

	if (ret)
		alog("os_info.c: ERROR: An error has occured while reloading the configuration file");

	return MOD_CONT;
}

/*************************************************************************/

/* This help will be added to the main NickServ list */
void mMainNickHelp(User *u)
{
	if (is_oper(u))
		me->NoticeLang(s_NickServ, u, OINFO_HELP_CMD);
}

/* This help will be added to the main ChanServ list */
void mMainChanHelp(User *u)
{
	if (is_oper(u))
		me->NoticeLang(s_ChanServ, u, OCINFO_HELP_CMD);
}

/*************************************************************************/

MODULE_INIT("os_info", OSInfo)
