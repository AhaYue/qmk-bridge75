
int main(void) {
    USB_DRIVER.state = USB_SUSPENDED;
    USB_DRIVER.status = 0;
    key = true;
    usb_remote_wakeup();
    now = 7001;
    usb_remote_wakeup();
    now = 14002;
    usb_remote_wakeup();
    assert(downs == 1 && wakes == 0);

    USB_DRIVER.status = 2;
    usb_remote_wakeup();
    usb_remote_wakeup();
    assert(wakes == 1);

    // Switching away resets this suspend even if the driver still reports it.
    transport = TRANSPORT_WLS;
    usb_remote_wakeup();
    transport = TRANSPORT_USB;
    usb_remote_wakeup();
    assert(downs == 1 && wakes == 2);
    now += 7001;
    usb_remote_wakeup();
    assert(downs == 2 && wakes == 2);

    USB_DRIVER.state = 1;
    usb_remote_wakeup();
    USB_DRIVER.state = USB_SUSPENDED;
    usb_remote_wakeup();
    assert(downs == 2 && wakes == 3);
    puts("PASS USB: host wake permission, one request and powerdown per suspend, state reset on USB exit");
}
