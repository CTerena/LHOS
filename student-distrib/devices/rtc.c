#include "rtc.h"
#include "../lib.h"
#include "../i8259.h"

uint32_t rtc_time_counter;

/* struct storing frequency and counter for process with
different frequencies. Used to implement virtualization */
typedef struct {
    int32_t proc_exist;
    int32_t proc_freq;
    int32_t proc_count;
} proc_freqcount_pair;

static proc_freqcount_pair RTC_proc_list[MAX_PROC_NUM];

/* RTC_init - Initialization of Real-Time Clock (RTC)
 * 
 * Initializes the RTC to a base frequency of 1024.
 *  
 * Inputs: none
 * Outputs: none (Configuration of RTC registers)
 * Side Effects: Modifies RTC control registers A and B. Resets the rtc_time_counter and enables
 *               the RTC interrupt request line (IRQ).
 */
void RTC_init(void) {
    printf("RTC_init\n");
    char prev;
    outb(RTC_B, RTC_PORT);     // select register B, and disable NMI
    prev = inb(RTC_CMOS_PORT); // read the current value of register B
    outb(RTC_B, RTC_PORT);     // set the index again (inb will reset the idx)
    /* write the previous value ORed with 0x40. This turns on bit 6 of register B */
    outb(prev | 0x40, RTC_CMOS_PORT);

    outb(RTC_A, RTC_PORT);     // set index to register A, disable NMI
    prev = inb(RTC_CMOS_PORT); // get the previous value of register B
    outb(RTC_A, RTC_PORT);     // set the index again
    outb((prev & 0xF0) | RTC_BASE_RATE, RTC_CMOS_PORT);  // set the frequency to 2 Hz

    rtc_time_counter = 0;
    enable_irq(RTC_IRQ);
}


/* __intr_RTC_handler - Real-Time Clock (RTC) Interrupt Handler
 * 
 * Handles interrupts generated by the RTC.
 * 
 * Inputs: None (Triggered by RTC interrupt)
 * Outputs: None (Handles interrupt side effects)
 * Side Effects: Increments rtc_time_counter. Acknowledges RTC interrupt by reading from register C.
 *               Sends EOI to the RTC IRQ. Potentially triggers other interrupt handlers through the
 *               test_interrupts() function.
 */
void __intr_RTC_handler(void) {
    cli();
    rtc_time_counter ++;
    int32_t pid;
    /* update each process's counter */
    for (pid = 0; pid < MAX_PROC_NUM; ++pid) {
            RTC_proc_list[pid].proc_count --;
    }
    outb(RTC_C &0x0F, RTC_PORT); // select register C
    inb(RTC_CMOS_PORT);		    // just throw away contents

    send_eoi(RTC_IRQ);
    sti();
}

/* 
 * Function: RTC_open
 * Description: Initializes the RTC interrupt handler for a specific process, 
 * setting the RTC frequency to 2Hz and validating the process's ID.
 * Parameters:
 *    proc_id: id of the process
 * Returns: 0 for success
 * Side Effects: 1. sets the RTC frequency for the process to 2Hz
 *               2. validates the process's id
 */
int32_t RTC_open(int32_t proc_id) {
    /* set process's freq and existence status */
    RTC_proc_list[proc_id].proc_freq = 2;
    RTC_proc_list[proc_id].proc_count = RTC_BASE_FREQ / 2; 
    RTC_proc_list[proc_id].proc_exist = 1;
    return 0;
}

/* 
 * Function: RTC_close
 * Description: invalidates the process's RTC by 
 * marking its id as invalid.
 * Parameters:
 *    proc_id: id of the process
 * Returns: 0 for success
 * Side Effects: set the process's id invalid
 */
int32_t RTC_close(int32_t proc_id) {
    /* invalidate the process's existence status */
    RTC_proc_list[proc_id].proc_exist = 0;
    return 0;
}

/* 
 * Function: RTC_read
 * Description: block until the next interrupt occurs for the process
 * Parameters:
 *    buf: ignored
 *    nbytes: ignored
 *    proc_id: id of the process
 * Returns:
 *    0 for success
 * Side Effects: block until the next interrupt occurs for the process
 */
int32_t RTC_read(void* buf, int32_t nbytes, int32_t proc_id) {
    /* virtualization: wait counter reaches zero */
    while(RTC_proc_list[proc_id].proc_count);
    /* reset counter */
    if(RTC_proc_list[proc_id].proc_freq)
        RTC_proc_list[proc_id].proc_count = RTC_BASE_FREQ / RTC_proc_list[proc_id].proc_freq;
    return 0;
}

/* 
 * Function: RTC_write
 * Description: adjusts the RTC frequency for a specific process
 * Parameters:
 *    buf: ptr to the value of the intended frequency
 *    nbytes: ignored
 *    proc_id: Identifier ID of the process
 * Returns:
 *    0 for success, -1 for invalid arguments
 * Side Effects: adjusts the freq for the process
 */
int32_t RTC_write(void* buf, int32_t nbytes, int32_t proc_id) {
    uint32_t freq = *(uint32_t*) buf;
    /* ensuring freq is a power of 2 and within acceptable limits */
    if(!(freq && !(freq & (freq - 1))) || freq > 1024) {
        printf("chidafenla");
        return -1;
    }
    /* adjusts the freq */
    RTC_proc_list[proc_id].proc_freq = freq;
    RTC_proc_list[proc_id].proc_count = RTC_BASE_FREQ / RTC_proc_list[proc_id].proc_freq;
    return 0;
}
