#include <asf.h>
#include <stdio.h>
#include <ioport.h>
#include <board.h>

void setup_timer(void);
void incr(void);

int score = 0;
int incremental = 0;
static char buffarray[200];

/**
 * @brief Mengaktifkan timer/counter 0 di port C untuk sensor ultrasonik.
 * 
 */
void setup_timer(void) {
	// Mengaktifkan modul timer/counter 0 di port C
	tc_enable(&TCC0);
	// Membuat fungsi incr sebagai fungsi callback saat timer/counter interrupt overflow
	tc_set_overflow_interrupt_callback(&TCC0, incr);
	// Mengkonfigurasi timer/counter di mode TC_WG_NORMAL
	tc_set_wgm(&TCC0, TC_WG_NORMAL);
	// Set periode timer menjadi 58
	tc_write_period(&TCC0, 58);
	// Mengkonfigurasi level timer/counter interrupt overflow
	tc_set_overflow_interrupt_level(&TCC0, TC_INT_LVL_HI);
	// Mengkonfigurasi sumber timer/counter
	tc_write_clock_source(&TCC0, TC_CLKSEL_DIV1_gc);
}

/**
 * @brief Menambah variabel incremental dengan 1 setiap ~29us menggunakan
 * timer/counter.
 *
 */
void incr(void) {
	incremental += 1;
}

/**
 * @brief Fungsi utama yang akan dijalankan
 * 
 * @return int 
 */
int main (void) {
	// Menginisialisasikan board, system clock, dan semua level interrupt (PMIC)
	board_init();
	sysclk_init();
	pmic_init();
	gfx_mono_init();

	// Menyalakan backlight LCD board
	gpio_set_pin_high(NHD_C12832A1Z_BACKLIGHT);
	
	gfx_mono_draw_string("Fathan-Fikri-Vincent", 0, 0, &sysfont);
	
	// Mengaktifkan RTC32 system clock
	sysclk_enable_module(SYSCLK_PORT_GEN, SYSCLK_RTC);

	// Menunggu RTC32 system clock untuk menjadi stabil
	while (RTC32.SYNCCTRL & RTC32_SYNCBUSY_bm) { }

	// Delay 1 detik dan mengaktifkan timer/counter untuk sensor ultrasonik
	delay_ms(1000);
	setup_timer();

	// Menetapkan port E sebagai output
	PORTE.DIR = 0xFF;
	
	int locked = 0;	 // Untuk memantau apakah rumah sedang dalam keadaan terkunci
	
	// Menetapkan J1 Pin 0 sebagai output (untuk menggunakan tombol di board)
	ioport_set_pin_dir(J1_PIN0, IOPORT_DIR_OUTPUT);
	
	// Superloop utama
	while (1) {
		tc_enable(&TCC0);

		// Set port B (untuk sensor ultrasonik) sebagai output untuk memancarkan suara ultrasonik
		PORTB.DIR = 0xFF;

		// Ouput pancaran suara ultrasonik
		PORTB.OUT = 0x00;
		PORTB.OUT = 0xFF;
		delay_us(5);

		// Set port B sebagai input untuk menerima kembali pantulan suara ultrasonik
		PORTB.OUT = 0x00;
		PORTB.DIR = 0x00;
		delay_us(750);

		// Simpan nilai incremental awal
		int oldinc = incremental;
		delay_us(115);

		// Lakukan interrupt
		cpu_irq_enable();

		// Tunggu hingga prot B menerima input dari sensor ultrasonik yang menerima
		// pantulan dari suara ultrasonik yang dipancarkannya
		while (PORTB.IN & PIN0_bm) { }

		// Simpan nilai incremental setelah sensor ultrasonik menerima pantulan suara ultrasonik
		int newinc = incremental;

		// Matikan interrupt
		cpu_irq_disable();

		// Batas jarak sensor hingga ~300cm
		if (incremental > 300) {
			score = 300;
			// Cetak di LCD jarak dari sensor ultrasonik ke objek yang terdeteksi di depannya
			snprintf(buffarray, sizeof(buffarray), "Jarak: %d cm  ", score);
			gfx_mono_draw_string(buffarray, 0, 24, &sysfont);
		}
		else {
			// Selisih waktu antara saat suara ultrasonik dipancarkan sensor
			// hingga pantulannya diterima kembali oleh sensor,
			int inc = newinc - oldinc;
			int newscore = inc * 1.1538461538461;

			// Rumah dalam keadaan terkunci
			if (locked) {
				// Tidak ada objek hingga 20cm di depan sensor ultrasonik
				if (newscore > 20) {
					PORTE.OUT = 0b00011000;
					gfx_mono_draw_string("Rumah aman           ", 0, 16, &sysfont);
				}
				// Terdapat objek di antara 5cm hingga 20cm di depan sensor ultrasonik
				else if (newscore > 5) {
					PORTE.OUT = 0b00011000;
					gfx_mono_draw_string("Ada yang mencurigakan", 0, 16, &sysfont);
				}
				// Terdapat objek kurang dari 5cm di depan sensor ultrasonik
				else {
					PORTE.OUT = 0b00011010;  // Aktifkan port E1 untuk menyalakan buzzer
					delay_ms(50);
					gfx_mono_draw_string("BAHAYA!!!!!!         ", 0, 16, &sysfont);
					PORTE.OUT = 0b00011000;
				}
				gfx_mono_draw_string("Rumah terkunci", 0, 8, &sysfont);
			}
			// Rumah dalam keadaan terbuka
			else {
				gfx_mono_draw_string("Rumah terbuka ", 0, 8, &sysfont);
				gfx_mono_draw_string("                     ", 0, 16, &sysfont);
			}

			// Cetak di LCD jarak dari sensor ultrasonik ke objek yang terdeteksi di depannya
			snprintf(buffarray, sizeof(buffarray), "Jarak: %d cm  ", newscore);
			gfx_mono_draw_string(buffarray, 0, 24, &sysfont);
		}
		
		// Reset incremental
		incremental = 0;
		
		// Jika tombol 1 di board ditekan, toggle kunci rumah
		if (ioport_get_pin_level(GPIO_PUSH_BUTTON_1) == 0){
			locked = (locked==0) ? 1 : 0;
		}
	}
}
