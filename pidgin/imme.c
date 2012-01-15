#define PURPLE_PLUGINS

#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <sys/inotify.h>
#include "plugin.h"
#include "version.h"
#include "account.h"

static PurpleAccount *pa = NULL;
static PurpleBuddy *pb = NULL;
static PurpleConversation *pc = NULL;
static int fd, wd;

void on_deleting_conversation (PurpleConversation *conv);
void on_deleting_conversation (PurpleConversation *conv)
{
	if (pc == conv) pc = NULL;
}

void process_fs_event (gpointer arg, gint gfd, PurpleInputCondition cond);
void process_fs_event (gpointer arg, gint gfd, PurpleInputCondition cond)
{
	int br, i;
	char msg[256];
	char buff[4096];
	struct inotify_event *ie;

	if (cond != PURPLE_INPUT_READ) return;
	memset(buff, 0, 4096);

	br = read (gfd, buff, 4096);
	i = 0;
	while (i < br)
	{
		ie = (struct inotify_event *)&buff[i];
		sprintf (msg, "br = %d, %s was %s", br, ie-> name, ie->mask & IN_MODIFY ? "modified" : "created");

		if (!pc) pc = purple_conversation_new (PURPLE_CONV_TYPE_IM, pa, pb->name);
		if (pc) purple_conv_im_send(PURPLE_CONV_IM(pc), msg);
		i += sizeof(struct inotify_event) + ie->len;
	}
}

static gboolean imme_load(PurplePlugin* plugin)
{
	GList *accounts = purple_accounts_get_all_active();
	GList *account = accounts;
	GSList *buddies, *buddy;

	while (account)
	{
		pa = (PurpleAccount*) account->data;
		if (g_strstr_len(pa->username, 20, purple_prefs_get_string("/plugins/imme/from")))
			break;
		account = account->next;
	}

	if (!pa) return FALSE;

	buddies = purple_find_buddies(pa, NULL);
	buddy = buddies;
	while (buddy)
	{
		pb = (PurpleBuddy*) buddy->data;
		if (g_strstr_len(pb->name, 20, purple_prefs_get_string("/plugins/imme/to")))
			break;
		buddy = buddy->next;
	}

	if (!pb) return FALSE;

	fd = inotify_init();
	if (fd < 0)
		return FALSE;
	wd = inotify_add_watch(fd, purple_prefs_get_string("/plugins/imme/dirname"), IN_MODIFY | IN_CREATE);

	purple_signal_connect (purple_conversations_get_handle(),
							"deleting-conversation",
							plugin,
							PURPLE_CALLBACK(on_deleting_conversation),
							NULL);


	purple_input_add(fd, PURPLE_INPUT_READ, process_fs_event, NULL);

	return TRUE;
}


static void imme_destroy(PurplePlugin* plugin)
{
	inotify_rm_watch(fd, wd);
}

static gboolean imme_unload(PurplePlugin* plugin)
{
	imme_destroy(plugin);
	return TRUE;
}

static PurplePluginInfo imme_info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,
	"gtk-autif-imme",
	"IM me",
	"0.1",
	"IM me",
	"IM me when something happens",
	NULL,
	NULL,
	imme_load,
	imme_unload,
	imme_destroy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void imme_init(PurplePlugin* plugin)
{
	purple_prefs_add_none("/plugins/imme");
	//purple_prefs_add_string ("/plugins/imme/from", "tekxtc@gmail.com");
	//purple_prefs_add_string ("/plugins/imme/to", "autifkhan@gmail.com");
	purple_prefs_add_string ("/plugins/imme/from", "khan@ubserver.ub");
	purple_prefs_add_string ("/plugins/imme/to", "autif@ubserver.ub");
	purple_prefs_add_string ("/plugins/imme/dirname", "/home/autif/data/tmp/pidgin/xfr");
}

PURPLE_INIT_PLUGIN (gtk-autif-imme, imme_init, imme_info);

