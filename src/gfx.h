/* See LICENSE for license information. */

/**
 * Gfx - renderer interface
 */

#pragma once


#include <stdbool.h>
#include <stdint.h>

#include "ui.h"
#include "util.h"
#include "vt.h"


/*
typedef struct {
    enum ContextType {
        CONTEXT_TYPE_GL_COMPAT,
        CONTEXT_TYPE_GL_CORE,
        CONTEXT_TYPE_GL_ES,
        CONTEXT_TYPE_VK,
    } type;
    
    uint8_t major;
    uint8_t minor;
} ContextInfo;
*/

typedef struct
{
    struct IGfx* interface;

    __attribute__((aligned(8))) uint8_t extend_data;

} Gfx;

struct IGfx
{
    void (*draw)                        (Gfx* self, const Vt*, Ui* ui);
    void (*resize)                      (Gfx* self, uint32_t w, uint32_t h);
    Pair_uint32_t (*get_char_size)      (Gfx* self);
    void (*init_with_context_activated) (Gfx* self);
    void (*reload_font)                 (Gfx* self);
    bool (*update_timers)               (Gfx* self, Vt* vt, Ui* ui, TimePoint** out_pending);
    void (*notify_action)               (Gfx* self);
    bool (*set_focus)                   (Gfx* self, bool in_focus);
    void (*flash)                       (Gfx* self);
    Pair_uint32_t (*pixels)             (Gfx* self, uint32_t rows, uint32_t columns);
    void (*destroy)                     (Gfx* self);
    void (*destroy_proxy)               (Gfx* self, int32_t proxy[static 4]);
};

static void Gfx_draw(Gfx* self, const Vt* vt, Ui* ui)
{
    self->interface->draw(self, vt, ui);
}

/**
 * Set window dimensions */
static void Gfx_resize(Gfx* self, uint32_t w, uint32_t h)
{
    self->interface->resize(self, w, h);
}

/**
 * Get the number of cells that can be drawn on the window with current dimensions */
static Pair_uint32_t Gfx_get_char_size(Gfx* self)
{
    return self->interface->get_char_size(self);
}

/**
 * Initialize renderer (requires an activated graphics context) */
static void Gfx_init_with_context_activated(Gfx* self)
{
    self->interface->init_with_context_activated(self);
}

static void Gfx_reload_font(Gfx* self)
{
    self->interface->reload_font(self);
}

static bool Gfx_update_timers(Gfx* self, Vt* vt, Ui* ui, TimePoint** out_pending)
{
    return self->interface->update_timers(self, vt, ui, out_pending);
}

static void Gfx_notify_action(Gfx* self)
{
    self->interface->notify_action(self);
}

static bool Gfx_set_focus(Gfx* self, bool in_focus)
{
    return self->interface->set_focus(self, in_focus);
}

static void Gfx_flash(Gfx* self)
{
    self->interface->flash(self);
}

/**
 * Get the number pixels required to fit a given number of cells */
static Pair_uint32_t Gfx_pixels(Gfx* self, uint32_t rows, uint32_t columns)
{
    return self->interface->pixels(self, rows, columns);
}

static void Gfx_destroy(Gfx* self)
{
    self->interface->destroy(self);
    free(self);
}

/**
 * Destroy the generated line 'proxy' object */
static void Gfx_destroy_proxy(Gfx* self,  int32_t proxy[static 4])
{
    self->interface->destroy_proxy(self, proxy);
}

