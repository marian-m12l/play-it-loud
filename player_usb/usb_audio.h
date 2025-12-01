#pragma once

void usb_audio_init();
void usb_audio_tasks();
void usb_audio_read_samples(int16_t* buffer, int length);
