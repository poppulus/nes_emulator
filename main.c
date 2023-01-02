//#include "SDL_FontCache.h"
//#include <SDL2/SDL.h>

#include <gtk/gtk.h>
#include "emu.h"

static cairo_surface_t *surface = NULL;

static void close_window()
{
    if (surface) cairo_surface_destroy(surface);
}

static void clear_surface()
{
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_paint(cr);

    cairo_destroy(cr);
}

static void draw_cb(GtkDrawingArea *d_area, cairo_t *cr, int w, int h, gpointer data)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
}

static void resize_cb(GtkWidget *widget, int w, int h, gpointer data)
{
    if (surface)
    {
        cairo_surface_destroy(surface);
        surface = NULL;
    }

    GdkSurface *t_surface = gtk_native_get_surface(gtk_widget_get_native(widget));

    if (t_surface)
    {
        surface = gdk_surface_create_similar_surface(t_surface, 
                                                    CAIRO_CONTENT_COLOR,
                                                    gtk_widget_get_width(widget),
                                                    gtk_widget_get_height(widget));

        clear_surface();
    }
}

static void open_response(GtkDialog *dialog, int res)
{
    if (res == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

        GFile *file = gtk_file_chooser_get_file(chooser);

        char *content;
        unsigned long size;

        if (g_file_load_contents(file, NULL, &content, &size, NULL, NULL))
        {
            printf("size: %ld\n", size);
            e_file_handler(content, size);
            free(content);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void open_activated(GSimpleAction *action, GVariant *parameter, gpointer app)
{
    GtkWidget *dialog;
    GtkWindow *window;
    GtkFileChooserAction f_action = GTK_FILE_CHOOSER_ACTION_OPEN;

    window = gtk_application_get_active_window(app);

    dialog = gtk_file_chooser_dialog_new(
        "Open File", window, f_action, 
        "_Cancel", GTK_RESPONSE_CANCEL, 
        "_Open", GTK_RESPONSE_ACCEPT, NULL);

    g_signal_connect(
        dialog, "response", G_CALLBACK(open_response), NULL);

    gtk_widget_show(dialog);
}

static void quit_activated(GSimpleAction *action, GVariant *parameter, gpointer app)
{
    g_application_quit(G_APPLICATION(app));
}

void activate(GtkApplication *app, gpointer data)
{
    GtkWidget *window;
    //GtkWidget *grid;
    GtkWidget *frame;
    GtkWidget *drawing_area;

    GSimpleAction *act_open, *act_quit;

    GMenu *menu_bar, *menu;
    GMenuItem *menu_open, *menu_quit;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "NES Emulator");

    g_signal_connect(window, "destroy", G_CALLBACK(close_window), NULL);

    //grid = gtk_grid_new();
    //gtk_window_set_child(GTK_WINDOW(window), grid);

    menu_bar = g_menu_new();
    menu = g_menu_new();

    menu_open = g_menu_item_new("Open", "app.open");
    g_menu_append_item(menu, menu_open);

    menu_quit = g_menu_item_new("Quit", "app.quit");
    g_menu_append_item(menu, menu_quit);

    g_menu_append_submenu(menu_bar, "File", G_MENU_MODEL(menu));

    frame = gtk_frame_new(NULL);
    gtk_window_set_child(GTK_WINDOW(window), frame);

    //gtk_grid_attach(GTK_GRID(grid), frame, 0, 0, 1, 1);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 320, 240);
    gtk_frame_set_child(GTK_FRAME(frame), drawing_area);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_cb, NULL, NULL);
    g_signal_connect_after(drawing_area, "resize", G_CALLBACK(resize_cb), NULL);

    /*
    GActionEntry app_entries[] = {
        {"open", open_activated, NULL, NULL, NULL},
        {"quit", quit_activated, NULL, NULL, NULL}
    };
    */

    //g_action_map_add_action_entries(
    //    G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    act_open = g_simple_action_new("open", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_open));
    g_signal_connect(act_open, "activate", G_CALLBACK(open_activated), app);

    act_quit = g_simple_action_new("quit", NULL);
    g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(act_quit));
    g_signal_connect(act_quit, "activate", G_CALLBACK(quit_activated), app);

    gtk_application_set_menubar(app, G_MENU_MODEL(menu_bar));
    gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(window), true);
    
    gtk_widget_show(window);
}

int main(int argc, char *argv[])
{
    GtkApplication *app = gtk_application_new(
        "org.gtk.nes_emulator", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}
