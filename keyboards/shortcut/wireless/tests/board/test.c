static void simulate_protocol_post_init(void) { active_driver = &usb_driver; event('P'); }

int main(void) {
    // A saved wireless selection must survive the later QMK USB-driver install.
    eeconfig_init_kb();
    confinfo.devs = DEVS_BT2;
    eeconfig_update_kb(confinfo.raw);
    keyboard_post_init_kb();
    assert(changes == 0 && names == 0 && commands == 0);
    assert(post_user == 1 && init_user == 1);
    assert(renderer_enabled && !renderer_suspended && !rail_high);
    assert(renderer_mode == RGB_MATRIX_SOLID_COLOR && hsv[0] == 0 && hsv[1] == 0 && hsv[2] == 0);
    simulate_protocol_post_init();
    assert(active_driver == &usb_driver);
    wireless_post_task();
    assert(active_driver == &wireless_driver && current_device == DEVS_BT2);
    assert(changes == 1 && names == 1 && commands == 0);
    assert(event_count == 4 && memcmp(events, "IPDN", 4) == 0);
    wireless_post_task();
    assert(changes == 1 && names == 1 && commands == 0);
    now += 100;
    wireless_post_task();
    assert(commands == 3 && changes == 1 && names == 1);
    now += 100;
    wireless_post_task();
    assert(commands == 3);
    puts("PASS board: initialization defers transport until after protocol_post_init, device before name, startup runs once");

    renderer_enabled = false;
    renderer_suspended = true;
    rail_high = true;
    current_device = DEVS_BT3;
    wireless_devs_change_kb(DEVS_BT2, DEVS_BT3, false);
    assert(renderer_enabled && !renderer_suspended && !rail_high);
    assert(confinfo.devs == DEVS_BT3);
    puts("PASS board: initialization restores RGB renderer, mode and background; device selection restores renderer and LED rail");
    default_layer_state = 1U << 7;
    keyboard_post_init_kb();
    assert(default_layer_state == 1 && saved_default_layer == 1);
    default_layer_state = 1U << 2;
    keyboard_post_init_kb();
    assert(default_layer_state == (1U << 2));
    default_layer_state = (1U << 0) | (1U << 2);
    keyboard_post_init_kb();
    assert(default_layer_state == 1);
    default_layer_state = 0;
    keyboard_post_init_kb();
    assert(default_layer_state == 1);
    puts("PASS board: invalid, zero and mixed persistent default layers normalize; valid Mac base preserved");
    wireless_post_task(); // consume the last initialization scenario
    // Zero is a real clock value, not an absent timestamp.
    unsigned before_commands = commands;
    now = 0;
    md_recovery_kb();
    wireless_post_task();
    assert(commands == before_commands);
    now = 99;
    wireless_post_task();
    assert(commands == before_commands);
    now = 100;
    wireless_post_task();
    assert(commands == before_commands + 3);
    now = 101;
    wireless_post_task();
    assert(commands == before_commands + 3);

    before_commands = commands;
    now = UINT32_MAX - 30;
    md_recovery_kb();
    wireless_post_task();
    assert(commands == before_commands);
    now = 68; // 99 ms after scheduling, across wrap.
    wireless_post_task();
    assert(commands == before_commands);
    now = 69;
    wireless_post_task();
    assert(commands == before_commands + 3);
    puts("PASS board: module configuration waits 100 ms at time zero and across uint32 timer wrap");

    confinfo.devs = DEVS_BT4;
    eeconfig_update_kb(confinfo.raw);
    keyboard_post_init_kb();
    assert(confinfo.devs == DEVS_USB);
    startup_pending = false;
    current_device = DEVS_BT1;
    pairing_key = BT_HOST1;
    output_pending = true;
    strcpy(output_text, "diagnostics from the previous host");
    output_pos = 1;
    unsigned before_changes = changes;
    keyrecord_t record = {.event.pressed = false};
    assert(!process_record_kb(BT_USB, &record));
    assert(changes == before_changes && current_device == DEVS_BT1);
    record.event.pressed = true;
    assert(!process_record_kb(BT_USB, &record));
    assert(changes == before_changes + 1 && current_device == DEVS_USB);
    assert(pairing_key == KC_NO && !output_pending && !output_text[output_pos]);
    assert(!process_record_kb(BR_DIAG, &record));
    assert(output_pending && output_kind == OUTPUT_DIAG);
    assert(!process_record_kb(BR_BAT, &record));
    assert(output_pending && output_kind == OUTPUT_BAT);
    output_pending = false;
    output_kind    = OUTPUT_NONE;
    confinfo.module_sleep = true;
    eeconfig_update_kb(confinfo.raw);
    assert(!process_record_kb(EE_CLR, &record));
    assert(!confinfo.module_sleep && confinfo.devs == DEVS_USB);
    assert(renderer_enabled && renderer_mode == RGB_MATRIX_SOLID_COLOR && hsv[2] == 0);
    puts("PASS board: unsupported saved host, USB key release, cancelled pairing and output on switch, EEPROM reset LEDs");
    (void)indicator_frames;
    (void)output_timer;
    (void)gpio_write_pin_high;
    return 0;
}
