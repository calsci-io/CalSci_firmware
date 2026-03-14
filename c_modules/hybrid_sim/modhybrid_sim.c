/*****************************************************************************
 * MicroPython C module: hybrid_sim
 *
 * Mirrors ST7565 framebuffer writes into a shadow buffer for desktop hybrid
 * simulation.
 *
 * Keep this module capture-only for the quick-fix path. Streaming hybrid
 * packets on the normal stdout/REPL stream makes the CDC console noisy and
 * breaks tools like mpremote.
 ****************************************************************************/

#include "py/obj.h"
#include "py/runtime.h"

#include "hybrid_sim_capture.h"

static mp_obj_t mp_hybrid_enable(size_t n_args, const mp_obj_t *args) {
    bool enabled = true;
    if (n_args == 1) {
        enabled = mp_obj_is_true(args[0]);
    }
    hybrid_sim_capture_enable(enabled);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_hybrid_enable_obj, 0, 1, mp_hybrid_enable);

static mp_obj_t mp_hybrid_enabled(void) {
    return mp_obj_new_bool(hybrid_sim_capture_enabled());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_enabled_obj, mp_hybrid_enabled);

static mp_obj_t mp_hybrid_reset(void) {
    hybrid_sim_capture_reset();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_reset_obj, mp_hybrid_reset);

static mp_obj_t mp_hybrid_frame_id(void) {
    return mp_obj_new_int_from_uint(hybrid_sim_capture_frame_id());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_frame_id_obj, mp_hybrid_frame_id);

static mp_obj_t mp_hybrid_read_fb(void) {
    uint8_t fb[HYBRID_SIM_FB_LEN];
    (void)hybrid_sim_capture_read_fb(fb, sizeof(fb));
    return mp_obj_new_bytes((const byte *)fb, sizeof(fb));
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_read_fb_obj, mp_hybrid_read_fb);

static mp_obj_t mp_hybrid_changed_since(mp_obj_t last_frame_in) {
    mp_int_t last_frame = mp_obj_get_int(last_frame_in);
    uint32_t frame_id = hybrid_sim_capture_frame_id();
    if (last_frame < 0) {
        return mp_const_true;
    }
    return mp_obj_new_bool(frame_id != (uint32_t)last_frame);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_hybrid_changed_since_obj, mp_hybrid_changed_since);

static mp_obj_t mp_hybrid_pop_frame(void) {
    mp_obj_t out = mp_obj_new_dict(0);
    uint32_t frame_id = hybrid_sim_capture_frame_id();
    uint8_t fb[HYBRID_SIM_FB_LEN];
    (void)hybrid_sim_capture_read_fb(fb, sizeof(fb));
    mp_obj_dict_store(out, MP_OBJ_NEW_QSTR(MP_QSTR_frame_id), mp_obj_new_int_from_uint(frame_id));
    mp_obj_dict_store(out, MP_OBJ_NEW_QSTR(MP_QSTR_fb), mp_obj_new_bytes((const byte *)fb, sizeof(fb)));
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_hybrid_pop_frame_obj, mp_hybrid_pop_frame);

static const mp_rom_map_elem_t hybrid_sim_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_hybrid_sim) },

    { MP_ROM_QSTR(MP_QSTR_enable), MP_ROM_PTR(&mp_hybrid_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_enabled), MP_ROM_PTR(&mp_hybrid_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&mp_hybrid_reset_obj) },

    { MP_ROM_QSTR(MP_QSTR_frame_id), MP_ROM_PTR(&mp_hybrid_frame_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_changed_since), MP_ROM_PTR(&mp_hybrid_changed_since_obj) },
    { MP_ROM_QSTR(MP_QSTR_read_fb), MP_ROM_PTR(&mp_hybrid_read_fb_obj) },
    { MP_ROM_QSTR(MP_QSTR_pop_frame), MP_ROM_PTR(&mp_hybrid_pop_frame_obj) },

    { MP_ROM_QSTR(MP_QSTR_WIDTH), MP_ROM_INT(HYBRID_SIM_WIDTH) },
    { MP_ROM_QSTR(MP_QSTR_HEIGHT), MP_ROM_INT(HYBRID_SIM_HEIGHT) },
    { MP_ROM_QSTR(MP_QSTR_PAGES), MP_ROM_INT(HYBRID_SIM_PAGES) },
    { MP_ROM_QSTR(MP_QSTR_FB_LEN), MP_ROM_INT(HYBRID_SIM_FB_LEN) },
};
static MP_DEFINE_CONST_DICT(hybrid_sim_module_globals, hybrid_sim_module_globals_table);

const mp_obj_module_t hybrid_sim_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&hybrid_sim_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_hybrid_sim, hybrid_sim_user_cmodule);
