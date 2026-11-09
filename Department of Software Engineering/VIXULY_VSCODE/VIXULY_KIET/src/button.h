/*
 * ATmega88PA - nut nhan va LED, lap trinh thanh ghi.
 * PC1 = RUN, PC3 = STOP: nhan HIGH, nha LOW (R3/R4 keo xuong ngoai).
 * PC2 = LED RUN, PC4 = LED STOP: sang HIGH. PD4 = nSLEEP qua motor.c.
 * Theo VIXULY_Logic: running = 1 la STOP, running = 0 la RUN.
 *
 * Goi motor_init() / motion_init() truoc button_init(), sau do bat ngat
 * (sei() neu bare-metal). Goi button_update() lien tuc trong loop,
 * nen cach nhau <= 5 ms; khong dung delay(250) trong main cu.
 * Chi goi cac API tu chuong trinh chinh, KHONG goi tu ISR.
 *
 * Thu vien so huu Timer1 / TIMER1_COMPA_vect: CTC, tick 1 ms.
 * ISR chi dem thoi gian; xu ly nut va motor_enable() o chuong trinh chinh.
 * Khong dung Arduino API; khong doi Timer0, Timer2, UART hay ngat encoder.
 * COM1A/B = 0 nen PB1/PB2 van dung duoc cho TFT.
 * Khong tat ngat >= 1 ms; khong dung thu vien khac chiem Timer1.
 *
 * Khoi dong STOP/POT. Phai nha ca hai nut truoc khi nhan RUN.
 * RUN ngan duoc xu ly khi nha; RUN giu 2 s chi phat mot su kien.
 * Trong menu: RUN ngan doi lua chon, RUN giu 2 s xac nhan.
 * STOP duoc xu ly ngay khi nhan on dinh 20 ms, huy menu va uu tien RUN.
 * Sau STOP phai nha ca hai nut roi moi nhan RUN lai.
 * LED chi bao trang thai chay/dung, khong nhap nhay khi chon che do.
 *
 * Khong goi motor_set(), motor_brake(), motion_update() khi STOP:
 * cac ham nay co the danh thuc driver. Khi dung PID, goi motion_stop()
 * khi chuyen sang STOP de reset PID va giu PD4 LOW.
 * Chi doc bien running, khong gan tu main va khong khai bao trung ten.
 */
#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>

/* Neu thay doi, dat build flags cho ca button.cpp va chuong trinh chinh. */
#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 20UL
#endif
#ifndef BUTTON_LONG_PRESS_MS
#define BUTTON_LONG_PRESS_MS 2000UL
#endif

/* Mac dinh dung khi vao menu; xac nhan xong van STOP, nhan RUN de chay.
 * Dat bang 0 neu muon giu trang thai RUN/STOP trong luc chon che do.
 */
#ifndef BUTTON_STOP_ON_SELECT
#define BUTTON_STOP_ON_SELECT 1
#endif

#define BUTTON_STOPPED   1U
#define BUTTON_RUNNING   0U
#define BUTTON_MODE_POT  0U
#define BUTTON_MODE_UART 1U

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t running;

void button_init(void);
void button_update(void);

/* Dung motor, huy lua chon chua xac nhan, cho nha ca hai nut. */
void button_stop(void);

/* Che do da xac nhan dung de chon nguon toc do PC0 / UART. */
uint8_t button_get_mode(void);
/* Che do tam thoi trong menu, chi dung de hien thi. */
uint8_t button_get_selected_mode(void);
uint8_t button_is_selecting(void);

#ifdef __cplusplus
}
#endif
#endif /* BUTTON_H */
