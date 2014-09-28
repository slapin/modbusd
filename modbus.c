#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <gio/gio.h>
#include <glib-unix.h>

#include <modbus.h>

modbus_t *ctx;
extern char *optarg;
extern int optind, opterr, optopt;
char serial[128];
int speed = 115200;
uint16_t regs[3];
int slave = 0xc;
static GMainLoop *loop = NULL;

static gboolean modbus_quit(gpointer data)
{
	return FALSE;
}

static void dbus_setreg(GDBusConnection *connection,
		       const gchar *sender_name,
		       const gchar *object_path,
		       const gchar *interface_name,
		       const gchar *signal_name,
		       GVariant *parameters,
		       gpointer userdata)
{
	int reg, val, ret;
	g_variant_get(parameters, "(ii)", &reg, &val, NULL);
	ret = modbus_write_register(ctx, reg, val);
}
static void dbus_getreg(GDBusConnection *connection,
		       const gchar *sender_name,
		       const gchar *object_path,
		       const gchar *interface_name,
		       const gchar *signal_name,
		       GVariant *parameters,
		       gpointer userdata)
{
}
int main(int argc, char *argv[])
{
	int ret;
	int opt;
	GError *err = NULL;
	GDbusConnection *conn;
	g_type_init();
	loop = g_main_loop_new(NULL, FALSE);
	conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &err);
	g_dbus_connection_signal_subscribe(conn, NULL, "ru.itetra.Modbus", "setreg", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
		dbus_setreg, NULL, NULL);
	g_dbus_connection_signal_subscribe(conn, NULL, "ru.itetra.Modbus", "getreg", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
		dbus_getreg, NULL, NULL);
	if (!conn) {
		g_printerr("Error connecting to D-Bus: %s\n", err->message);
		g_error_free(err);
		return 1;
	}
	strcpy(serial, "/dev/ttyUSB0");
	while ((opt = getopt(argc, argv, "p:s:")) != -1) {
		switch (opt) {
		case 'p':
			strncpy(serial, optarg, sizeof(serial));
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'l':
			slave = atoi(optarg);
			break;
		default:
			printf("usage: %s [-p port] [-s speed] [-l slave]\n", argv[0]);
			return -1;
		}
	}
	ctx = modbus_new_rtu(serial, speed, 'N', 8, 2);
	modbus_set_debug(ctx, TRUE);
	modbus_set_error_recovery(ctx,
			MODBUS_ERROR_RECOVERY_LINK |
			MODBUS_ERROR_RECOVERY_PROTOCOL);
	modbus_set_slave(ctx, slave);

	ret = modbus_connect(ctx);
	if (ret < 0) {
		printf("can't connect\n");
		return -1;
	}
	memset(&regs, 0, sizeof(regs));
	modbus_write_and_read_registers(ctx, 0, 3, regs, 0, 3, regs);
	g_unix_signal_add(SIGTERM, modbus_quit, NULL);
	g_unix_signal_add(SIGHUP, modbus_quit, NULL);
	g_main_loop_run(loop);
	return 0;
}
