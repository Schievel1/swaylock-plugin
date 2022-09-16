#ifndef _SWAYLOCK_H
#define _SWAYLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include <wayland-server-core.h>
#include "background-image.h"
#include "cairo.h"
#include "pool-buffer.h"
#include "seat.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "wayland-drm-client-protocol.h"

enum auth_state {
	AUTH_STATE_IDLE,
	AUTH_STATE_CLEAR,
	AUTH_STATE_INPUT,
	AUTH_STATE_INPUT_NOP,
	AUTH_STATE_BACKSPACE,
	AUTH_STATE_VALIDATING,
	AUTH_STATE_INVALID,
};

struct swaylock_colorset {
	uint32_t input;
	uint32_t cleared;
	uint32_t caps_lock;
	uint32_t verifying;
	uint32_t wrong;
};

struct swaylock_colors {
	uint32_t background;
	uint32_t bs_highlight;
	uint32_t key_highlight;
	uint32_t caps_lock_bs_highlight;
	uint32_t caps_lock_key_highlight;
	uint32_t separator;
	uint32_t layout_background;
	uint32_t layout_border;
	uint32_t layout_text;
	struct swaylock_colorset inside;
	struct swaylock_colorset line;
	struct swaylock_colorset ring;
	struct swaylock_colorset text;
};

struct swaylock_args {
	struct swaylock_colors colors;
	enum background_mode mode;
	char *font;
	uint32_t font_size;
	uint32_t radius;
	uint32_t thickness;
	uint32_t indicator_x_position;
	uint32_t indicator_y_position;
	bool override_indicator_x_position;
	bool override_indicator_y_position;
	bool ignore_empty;
	bool show_indicator;
	bool show_caps_lock_text;
	bool show_caps_lock_indicator;
	bool show_keyboard_layout;
	bool hide_keyboard_layout;
	bool show_failed_attempts;
	bool daemonize;
	bool indicator_idle_visible;
	char *plugin_command;
};

struct swaylock_password {
	size_t len;
	size_t buffer_len;
	char *buffer;
};

// for the plugin-based surface drawing
struct swaylock_bg_server {
	struct wl_display *display;
	struct wl_event_loop *loop;
	struct wl_global *wlr_layer_shell;
	struct wl_global *compositor;
	struct wl_global *shm;
	struct wl_global *xdg_output_manager;
	struct wl_global *zwp_linux_dmabuf;
	struct wl_global *drm;

	struct wl_client *surf_client;
};

struct dmabuf_modifier_pair {
	uint32_t format;
	uint32_t modifier_hi;
	uint32_t modifier_lo;
};

struct feedback_pair {
	uint32_t format;
	uint32_t unused_padding;
	uint32_t modifier_hi;
	uint32_t modifier_lo;
};

struct feedback_tranche  {
	dev_t tranche_device;
	struct wl_array indices;
	uint32_t flags;
};
struct dmabuf_feedback_state  {
	dev_t main_device;
	int table_fd;
	int table_fd_size;
	struct feedback_tranche *tranches;
	size_t tranches_len;
};

// todo: merge with swaylock_bg_server ?
struct forward_state {
	/* these pointers are copies of those in swaylock_state */
	struct wl_display *upstream_display;
	struct wl_registry *upstream_registry;

	struct wl_drm *drm;
	struct wl_shm *shm;
	/* this instance is used just for forwarding */
	struct zwp_linux_dmabuf_v1 *linux_dmabuf;
	/* list of wl_resources corresponding to (default/surface) feedback instances
	 * that should get updated when the upstream feedback is updated */
	struct wl_list feedback_instances;
	/* We only let the background generator create surfaces, but not
	 * subsurfaces, because those are much trickier to implement correctly,
	 * and a well designed background shouldn't need them anyway. */
	struct wl_compositor *compositor;

	uint32_t *shm_formats;
	uint32_t shm_formats_len;

	struct dmabuf_modifier_pair *dmabuf_formats;
	uint32_t dmabuf_formats_len;

	struct dmabuf_feedback_state current, pending;
	struct feedback_tranche pending_tranche;
};

/* this is a resource associated to a downstream wl_surface */
struct forward_surface {
	struct wl_surface *upstream;

	bool has_been_configured;
	struct wl_resource *layer_surface; // downstream only

	/* is null until get_layer_surface is called and initializes this */
	struct swaylock_surface *sway_surface;
};

struct swaylock_state {
	struct loop *eventloop;
	struct loop_timer *clear_indicator_timer; // clears the indicator
	struct loop_timer *clear_password_timer;  // clears the password buffer
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zwlr_input_inhibit_manager_v1 *input_inhibit_manager;
	struct wl_shm *shm;
	struct zwp_linux_dmabuf_feedback_v1 *dmabuf_default_feedback;
	struct wl_list surfaces;
	struct wl_list images;
	struct swaylock_args args;
	struct swaylock_password password;
	struct swaylock_xkb xkb;
	enum auth_state auth_state;
	int failed_attempts;
	bool run_display;
	struct ext_session_lock_manager_v1 *ext_session_lock_manager_v1;
	struct ext_session_lock_v1 *ext_session_lock_v1;
	struct zxdg_output_manager_v1 *zxdg_output_manager;
	struct forward_state forward;
	struct swaylock_bg_server server;
};

struct swaylock_surface {
	cairo_surface_t *image;
	struct swaylock_state *state;
	struct wl_output *output;
	uint32_t output_global_name;
	struct wl_surface *surface;
	struct wl_surface *child; // surface made into subsurface
	struct wl_subsurface *subsurface;
	// rendered by plugin, unsynchronized, placed between surface + child
	struct wl_subsurface *plugin_subsurface;
	struct forward_surface *plugin_child;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct ext_session_lock_surface_v1 *ext_session_lock_surface_v1;
	struct pool_buffer buffers[2];
	struct pool_buffer indicator_buffers[2];
	struct pool_buffer *current_buffer;
	bool frame_pending, dirty;
	uint32_t width, height;
	uint32_t indicator_width, indicator_height;
	int32_t scale;
	enum wl_output_subpixel subpixel;
	char *output_name;
	char *output_description;
	struct wl_list link;

	struct wl_global *nested_server_output;
	// todo: list of associated resources
	struct wl_list nested_server_wl_output_resources;
	struct wl_list nested_server_xdg_output_resources;
};

/* Forwarding interface. These create various resources which maintain an
 * exactly corresponding resource on the server side. (With exceptions:
 * wl_regions do not need to be forwarded, so such wl_region-type wl_resources
 * lack user data.
 *
 * This solution is only good for a prototype, because blind forwarding lets
 * a bad plugin process directly overload the compositor, instead of overloading
 * swaylock. The correct thing to do is to fully maintain local buffer/surface
 * state, and only upload buffer data or send damage as needed; it is better
 * to crash swaylock than to crash the compositor.
 */
void bind_wl_compositor(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void bind_wl_shm(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void bind_linux_dmabuf(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void bind_drm(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void send_dmabuf_feedback_data(struct wl_resource *feedback, const struct dmabuf_feedback_state *state);

// There is exactly one swaylock_image for each -i argument
struct swaylock_image {
	char *path;
	char *output_name;
	cairo_surface_t *cairo_surface;
	struct wl_list link;
};

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint);
void render_frame_background(struct swaylock_surface *surface);
void render_frame(struct swaylock_surface *surface);
void damage_surface(struct swaylock_surface *surface);
void damage_state(struct swaylock_state *state);
void clear_password_buffer(struct swaylock_password *pw);
void schedule_indicator_clear(struct swaylock_state *state);

void initialize_pw_backend(int argc, char **argv);
void run_pw_backend_child(void);
void clear_buffer(char *buf, size_t size);

#endif
