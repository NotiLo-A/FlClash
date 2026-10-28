#include "my_application.h"

#include <flutter_linux/flutter_linux.h>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include <window/window_plugin.h>

#include "flutter/generated_plugin_registrant.h"

struct _MyApplication {
  GtkApplication parent_instance;
  char** dart_entrypoint_arguments;
  gboolean had_window_before_emit;
  FlMethodChannel* cli_channel;
};

G_DEFINE_TYPE(MyApplication, my_application, GTK_TYPE_APPLICATION)

static const GOptionEntry kCommandLineEntries[] = {
    {"minimized", 'm', 0, G_OPTION_ARG_NONE, nullptr, "Start minimized to tray", nullptr},
    {"hidden", 0, 0, G_OPTION_ARG_NONE, nullptr, "Start hidden in tray", nullptr},
    {"start", 0, 0, G_OPTION_ARG_NONE, nullptr, "Start proxy service", nullptr},
    {"stop", 0, 0, G_OPTION_ARG_NONE, nullptr, "Stop proxy service", nullptr},
    {"toggle", 0, 0, G_OPTION_ARG_NONE, nullptr, "Toggle proxy service", nullptr},
    {"restart-core", 0, 0, G_OPTION_ARG_NONE, nullptr, "Restart core service", nullptr},
    {"status", 's', 0, G_OPTION_ARG_NONE, nullptr, "Print running status", nullptr},
    {"status-json", 0, 0, G_OPTION_ARG_NONE, nullptr, "Print running status as JSON", nullptr},
    {"show", 0, 0, G_OPTION_ARG_NONE, nullptr, "Show main window", nullptr},
    {"hide", 0, 0, G_OPTION_ARG_NONE, nullptr, "Hide main window", nullptr},
    {"quit", 'q', 0, G_OPTION_ARG_NONE, nullptr, "Quit running application", nullptr},
    {"version", 'v', 0, G_OPTION_ARG_NONE, nullptr, "Print application version", nullptr},
    {nullptr}
};

struct CliInvocation {
  GApplicationCommandLine* command_line;
  MyApplication* app;
  gchar* action;
};

static void on_cli_response(GObject* source_object, GAsyncResult* res, gpointer user_data) {
  CliInvocation* invocation = static_cast<CliInvocation*>(user_data);
  g_autoptr(GError) error = nullptr;
  g_autoptr(FlMethodResponse) response =
      fl_method_channel_invoke_method_finish(FL_METHOD_CHANNEL(source_object), res, &error);

  if (response != nullptr && FL_IS_METHOD_SUCCESS_RESPONSE(response)) {
    FlValue* result = fl_method_success_response_get_result(FL_METHOD_SUCCESS_RESPONSE(response));
    if (result != nullptr && fl_value_get_type(result) == FL_VALUE_TYPE_STRING) {
      const gchar* msg = fl_value_get_string(result);
      g_application_command_line_print(invocation->command_line, "%s\n", msg);
      if (g_strcmp0(invocation->action, "status") != 0 &&
          g_strcmp0(invocation->action, "status-json") != 0 &&
          g_strcmp0(invocation->action, "hide") != 0 &&
          g_strcmp0(invocation->action, "show") != 0) {
        g_autoptr(GNotification) notification = g_notification_new("FlClash");
        g_notification_set_body(notification, msg);
        g_application_send_notification(G_APPLICATION(invocation->app), "cli-status", notification);
      }
    }
    g_application_command_line_set_exit_status(invocation->command_line, 0);
  } else {
    const gchar* err_msg = error != nullptr ? error->message : "Command failed";
    g_application_command_line_printerr(invocation->command_line, "Error: %s\n", err_msg);
    g_application_command_line_set_exit_status(invocation->command_line, 1);
  }

  g_object_unref(invocation->command_line);
  g_free(invocation->action);
  g_free(invocation);
}

static gint my_application_handle_local_options(GApplication* application, GVariantDict* options) {
  if (g_variant_dict_contains(options, "version")) {
    g_print("FlClash 0.9.2\n");
    return 0;
  }
  return -1;
}

// Implements GApplication::activate.
static void my_application_activate(GApplication* application) {
  MyApplication* self = MY_APPLICATION(application);
  if (self->had_window_before_emit) {
    return;
  }
  GtkWindow* window =
      GTK_WINDOW(gtk_application_window_new(GTK_APPLICATION(application)));

  // Use a header bar when running in GNOME as this is the common style used
  // by applications and is the setup most users will be using (e.g. Ubuntu
  // desktop).
  // If running on X and not using GNOME then just use a traditional title bar
  // in case the window manager does more exotic layout, e.g. tiling.
  // If running on Wayland assume the header bar will work (may need changing
  // if future cases occur).
  gboolean use_header_bar = TRUE;
#ifdef GDK_WINDOWING_X11
  GdkScreen* screen = gtk_window_get_screen(window);
  if (GDK_IS_X11_SCREEN(screen)) {
    const gchar* wm_name = gdk_x11_screen_get_window_manager_name(screen);
    if (g_strcmp0(wm_name, "GNOME Shell") != 0) {
      use_header_bar = FALSE;
    }
  }
#endif
  if (use_header_bar) {
    GtkHeaderBar* header_bar = GTK_HEADER_BAR(gtk_header_bar_new());
    gtk_widget_show(GTK_WIDGET(header_bar));
    gtk_header_bar_set_title(header_bar, "FlClash");
    gtk_header_bar_set_show_close_button(header_bar, TRUE);
    gtk_window_set_titlebar(window, GTK_WIDGET(header_bar));
  } else {
    gtk_window_set_title(window, "FlClash");
  }

  gtk_window_set_default_size(window, 680, 580);

  // X11 panels that do not match the window to a desktop entry read its icon;
  // GDK leaves one over 512x512 out of _NET_WM_ICON without a word.
  g_autofree gchar* executable = g_file_read_link("/proc/self/exe", nullptr);
  if (executable != nullptr) {
    g_autofree gchar* directory = g_path_get_dirname(executable);
    g_autofree gchar* path = g_build_filename(
        directory, "data", "flutter_assets", "assets", "images", "icon.png",
        nullptr);
    g_autoptr(GdkPixbuf) icon =
        gdk_pixbuf_new_from_file_at_size(path, 256, 256, nullptr);
    if (icon != nullptr) {
      gtk_window_set_icon(window, icon);
    }
  }

  g_autoptr(FlDartProject) project = fl_dart_project_new();
  fl_dart_project_set_dart_entrypoint_arguments(project, self->dart_entrypoint_arguments);

  FlView* view = fl_view_new(project);
  GdkRGBA background_color;
  gdk_rgba_parse(&background_color, "#000000");
  fl_view_set_background_color(view, &background_color);
  gtk_widget_show(GTK_WIDGET(view));
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

  // Realize the view so Flutter can render even if the window stays hidden
  // until Dart startup logic explicitly shows it.
  gtk_widget_realize(GTK_WIDGET(view));

  fl_register_plugins(FL_PLUGIN_REGISTRY(view));

  g_autoptr(FlPluginRegistrar) cli_registrar = fl_plugin_registry_get_registrar_for_plugin(
      FL_PLUGIN_REGISTRY(view), "FlClashCliPlugin");
  FlBinaryMessenger* messenger = fl_plugin_registrar_get_messenger(cli_registrar);
  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  self->cli_channel = fl_method_channel_new(messenger, "flclash/cli", FL_METHOD_CODEC(codec));

  gtk_widget_grab_focus(GTK_WIDGET(view));
}

// Implements GApplication::command_line.
static gint my_application_command_line(GApplication* application, GApplicationCommandLine* command_line) {
  MyApplication* self = MY_APPLICATION(application);
  GVariantDict* options = g_application_command_line_get_options_dict(command_line);

  const gchar* action = nullptr;
  if (g_variant_dict_contains(options, "start")) {
    action = "start";
  } else if (g_variant_dict_contains(options, "stop")) {
    action = "stop";
  } else if (g_variant_dict_contains(options, "toggle")) {
    action = "toggle";
  } else if (g_variant_dict_contains(options, "restart-core")) {
    action = "restart-core";
  } else if (g_variant_dict_contains(options, "status")) {
    action = "status";
  } else if (g_variant_dict_contains(options, "status-json")) {
    action = "status-json";
  } else if (g_variant_dict_contains(options, "show")) {
    action = "show";
  } else if (g_variant_dict_contains(options, "hide")) {
    action = "hide";
  } else if (g_variant_dict_contains(options, "quit")) {
    action = "quit";
  }

  if (self->cli_channel != nullptr) {
    if (action != nullptr) {
      if (g_strcmp0(action, "show") == 0) {
        window_plugin_activate();
        g_application_command_line_print(command_line, "Window raised\n");
        return 0;
      }
      CliInvocation* invocation = g_new0(CliInvocation, 1);
      invocation->command_line = G_APPLICATION_COMMAND_LINE(g_object_ref(command_line));
      invocation->app = self;
      invocation->action = g_strdup(action);

      fl_method_channel_invoke_method(self->cli_channel, action, nullptr, nullptr,
                                      on_cli_response, invocation);
      return 0;
    }
    window_plugin_activate();
    return 0;
  }

  if (action != nullptr && (g_strcmp0(action, "status") == 0 ||
                            g_strcmp0(action, "status-json") == 0 ||
                            g_strcmp0(action, "stop") == 0)) {
    g_application_command_line_printerr(command_line, "FlClash is not running.\n");
    return g_strcmp0(action, "stop") == 0 ? 0 : 1;
  }

  if (self->dart_entrypoint_arguments == nullptr) {
    gchar** arguments = g_application_command_line_get_arguments(command_line, nullptr);
    self->dart_entrypoint_arguments = g_strdupv(arguments + 1);
    g_strfreev(arguments);
  }
  g_application_activate(application);
  return 0;
}

// Implements GApplication::before_emit.
static void my_application_before_emit(GApplication* application, GVariant* platform_data) {
  MyApplication* self = MY_APPLICATION(application);
  self->had_window_before_emit =
      gtk_application_get_windows(GTK_APPLICATION(application)) != nullptr;
  G_APPLICATION_CLASS(my_application_parent_class)->before_emit(application, platform_data);
}

// Implements GApplication::after_emit.
static void my_application_after_emit(GApplication* application, GVariant* platform_data) {
  G_APPLICATION_CLASS(my_application_parent_class)->after_emit(application, platform_data);
}

// Implements GApplication::startup.
static void my_application_startup(GApplication* application) {
  G_APPLICATION_CLASS(my_application_parent_class)->startup(application);
}

// Implements GApplication::shutdown.
static void my_application_shutdown(GApplication* application) {
  G_APPLICATION_CLASS(my_application_parent_class)->shutdown(application);
}

// Implements GObject::dispose.
static void my_application_dispose(GObject* object) {
  MyApplication* self = MY_APPLICATION(object);
  g_clear_pointer(&self->dart_entrypoint_arguments, g_strfreev);
  g_clear_object(&self->cli_channel);
  G_OBJECT_CLASS(my_application_parent_class)->dispose(object);
}

static void my_application_class_init(MyApplicationClass* klass) {
  G_APPLICATION_CLASS(klass)->handle_local_options = my_application_handle_local_options;
  G_APPLICATION_CLASS(klass)->activate = my_application_activate;
  G_APPLICATION_CLASS(klass)->command_line = my_application_command_line;
  G_APPLICATION_CLASS(klass)->before_emit = my_application_before_emit;
  G_APPLICATION_CLASS(klass)->after_emit = my_application_after_emit;
  G_APPLICATION_CLASS(klass)->startup = my_application_startup;
  G_APPLICATION_CLASS(klass)->shutdown = my_application_shutdown;
  G_OBJECT_CLASS(klass)->dispose = my_application_dispose;
}

static void my_application_init(MyApplication* self) {
  self->cli_channel = nullptr;
  g_application_add_main_option_entries(G_APPLICATION(self), kCommandLineEntries);
}

MyApplication* my_application_new() {
  // Set the program name to the application ID, which helps various systems
  // like GTK and desktop environments map this running application to its
  // corresponding .desktop file. This ensures better integration by allowing
  // the application to be recognized beyond its binary name.
  g_set_prgname(APPLICATION_ID);

  return MY_APPLICATION(g_object_new(my_application_get_type(),
                                     "application-id", APPLICATION_ID,
                                     "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                                     nullptr));
}
