/*
 * hook.h: header for hook.c
 * 
 * Copyright 1990, 1995 Michael Sandrof, Matthew Green and others
 * Copyright 1997 EPIC Software Labs
 * See the COPYRIGHT file for license information.
 */

#ifndef __hook_h__
#define __hook_h__

#define FIRST_NAMED_HOOK 1000

enum HOOK_TYPES {
	ACTION_LIST = FIRST_NAMED_HOOK,
	CHANNEL_LOST_LIST,
	CHANNEL_NICK_LIST,
	CHANNEL_SIGNOFF_LIST,
	CHANNEL_SYNC_LIST,
	CONNECT_LIST,
	CTCP_LIST,
	CTCP_REPLY_LIST,
	CTCP_REQUEST_LIST,
	DCC_ACTIVITY_LIST,
	DCC_CHAT_LIST,
	DCC_CONNECT_LIST,
	DCC_LIST_LIST,
	DCC_LOST_LIST,
	DCC_OFFER_LIST,
	DCC_RAW_LIST,
	DCC_REQUEST_LIST,
	DISCONNECT_LIST,
	ENCRYPTED_NOTICE_LIST,
	ENCRYPTED_PRIVMSG_LIST,
	ERROR_LIST,
	EXEC_LIST,
	EXEC_ERRORS_LIST,
	EXEC_EXIT_LIST,
	EXEC_PROMPT_LIST,
	EXIT_LIST,
	FLOOD_LIST,
	GENERAL_NOTICE_LIST,
	GENERAL_PRIVMSG_LIST,
	HELP_LIST,
	HOOK_LIST,
	IDLE_LIST,
	INPUT_LIST,
	INVITE_LIST,
	JOIN_LIST,
	KEYBINDING_LIST,
	KICK_LIST,
	KILL_LIST,
	LIST_LIST,
	MAIL_LIST,
	MODE_LIST,
	MODE_STRIPPED_LIST,
	MSG_LIST,
	MSG_GROUP_LIST,
	NAMES_LIST,
	NEW_NICKNAME_LIST,
	NICKNAME_LIST,
	NOTE_LIST,
	NOTICE_LIST,
	NOTIFY_SIGNOFF_LIST,
	NOTIFY_SIGNON_LIST,
	NUMERIC_LIST,
	ODD_SERVER_STUFF_LIST,
	OPERWALL_LIST,
	OPER_NOTICE_LIST,
	PART_LIST,
	PONG_LIST,
	PUBLIC_LIST,
	PUBLIC_MSG_LIST,
	PUBLIC_NOTICE_LIST,
	PUBLIC_OTHER_LIST,
	RAW_IRC_LIST,
	REDIRECT_LIST,
	SEND_ACTION_LIST,
	SEND_CTCP_LIST,
	SEND_DCC_CHAT_LIST,
	SEND_MSG_LIST,
	SEND_NOTICE_LIST,
	SEND_PUBLIC_LIST,
	SEND_TO_SERVER_LIST,
	SERVER_ESTABLISHED_LIST,
	SERVER_LOST_LIST,
	SERVER_NOTICE_LIST,
	SERVER_STATUS_LIST,
	SET_LIST,
	SIGNAL_LIST,
	SIGNOFF_LIST,
	SILENCE_LIST,
	SSL_SERVER_CERT_LIST,
	STATUS_UPDATE_LIST,
	SWITCH_CHANNELS_LIST,
	SWITCH_QUERY_LIST,
	SWITCH_WINDOWS_LIST,
	TIMER_LIST,
	TOPIC_LIST,
	UNKNOWN_COMMAND_LIST,
	UNKNOWN_SET_LIST,
	UNLOAD_LIST,
	WALL_LIST,
	WALLOP_LIST,
	WHO_LIST,
	WINDOW_LIST,
	WINDOW_COMMAND_LIST,
	WINDOW_CREATE_LIST,
	WINDOW_BEFOREKILL_LIST,
	WINDOW_KILL_LIST,
	WINDOW_NOTIFIED_LIST,
	WINDOW_SERVER_LIST,
	YELL_LIST,
	ZZZZ_THIS_ALWAYS_COMES_LAST_ZZZZ
};

#define NUMBER_OF_LISTS ZZZZ_THIS_ALWAYS_COMES_LAST_ZZZZ
#define INVALID_HOOKNUM -1001

	BUILT_IN_COMMAND(oncmd);
	BUILT_IN_COMMAND(shookcmd);

	int	do_hook 		(int, const char *, ...) __A(2);
	int	do_hook_with_result	(int, char **, const char *, ...) __A(3);
	char *	hookctl			(char *);
	void	flush_on_hooks 		(void);
	void	unload_on_hooks		(char *);
	void	save_hooks 		(FILE *, int);
	void	do_stack_on		(int, char *);
	int	hook_find_free_serial	(int, int, int);

	extern int deny_all_hooks;

#define ADD_STR_TO_LIST(str, sep, item, len) if (len == 0) \
	{str = malloc_strdup(item); len = strlen(str);} \
	else malloc_strcat_wordlist_c(&str, sep, item, &len);
	
#endif /* __hook_h_ */
