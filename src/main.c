//#include <gpiod.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <gpiod.h>

#define CHIP_NAME "gpiochip0"

#define COUNT_S 4

#define LINE_S1 25
#define LINE_S2 10
#define LINE_S3 17
#define LINE_S4 18

#define COUNT_L 4

#define LINE_L1 24
#define LINE_L2 22
#define LINE_L3 23
#define LINE_L4 27

#define CLEAR(S) memset(&S, 0, sizeof(S))

volatile sig_atomic_t end = 1;

static void catch_sigint(int signo){
    end = 0;
}

void usage(const char *);
void coded_lock();

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (signal(SIGINT, catch_sigint) == SIG_ERR) { //thankyou wikipedia.org
        fputs("An error occurred while setting a signal handler.\n", stderr);
        return EXIT_FAILURE;
    }

    switch (atoi(argv[1]))
    {
    case 1:
        coded_lock();
        break;
    
    default:
        usage(argv[0]);
        return EXIT_FAILURE;
        break;
    }

    return 0;
}

void usage(const char * name) {
    printf("Usage: %s, [1-3]\n1 -> coded lock\n2 -> NI\n3 -> NI\n", name);
}

void coded_lock() {
    int ret, i, j, k, bad = 0;

    const int passwd[4] = {1,2,3,4};
    int entered[4] = {0,0,0,0};
    int pos = 0;

    struct gpiod_chip *chip;
    
    struct gpiod_line *line;
    unsigned int iter = 0;

    const struct timespec debounce_timer = { 0, 10000 }; 

    struct timespec timeout;

    struct gpiod_line_bulk event_lines;

    struct gpiod_line_bulk led = GPIOD_LINE_BULK_INITIALIZER;

    const int led_offsets[COUNT_L] = {LINE_L1, LINE_L2, LINE_L3, LINE_L4};
    int led_vals[COUNT_L] = {0,0,0,0};

    struct gpiod_line_bulk btn = GPIOD_LINE_BULK_INITIALIZER;

    const int btn_offsets[COUNT_S] = {LINE_S1, LINE_S2, LINE_S3, LINE_S4};
    int btn_vals[COUNT_S] = {0,0,0,0};
    int btn_vals_tmp[COUNT_S] = {0,0,0,0};

    CLEAR(timeout);
    timeout.tv_sec = 10;
    timeout.tv_nsec = 100000;

    chip = gpiod_chip_open_by_name(CHIP_NAME);

    if (chip == NULL){
        perror("Failed to open gpio device\n");
        goto end;
    }

    ret = gpiod_chip_get_lines(chip, led_offsets, COUNT_L, &led);
    if (ret < 0) {
        perror("Failed to get LED lines\n");
        goto free_chip;
    }

    ret = gpiod_chip_get_lines(chip, btn_offsets, COUNT_S, &btn);
    if (ret < 0) {
        perror("Failed to get button lines\n");
        goto free_chip;
    }        memset(&btn_vals, 0, sizeof(int)* COUNT_S);

    ret = gpiod_line_request_bulk_output(&led, "led-driver", led_vals);
    
    if (ret < 0) {
        perror("Failed to request led lines\n");
        goto free_chip;
    }

    ret = gpiod_line_request_bulk_falling_edge_events(&btn, "button-watch");

    if (ret < 0) {
        perror("Failed to request button lines\n");
        goto free_led;
    }

    while(end) {
        CLEAR(event_lines);
        printf("waiting on events\n");
	
        ret = gpiod_line_event_wait_bulk(&btn, &timeout, NULL);

        if(ret == 0) continue;

        if (ret < 0)
        {
            perror("error waiting on events\n");
            goto free_btn;
        }
        // This version of libgpoid doesn't support debouncing
        // so we have to do it ourselves
        printf("got events\n"); 
        memset(&btn_vals, 0, sizeof(int)* COUNT_S);
        memset(&btn_vals_tmp, 0, sizeof(int)* COUNT_S);
        
        ret = gpiod_line_get_value_bulk(&btn, btn_vals);
        if (ret < 0) {perror("Failed to read button lines"); goto free_btn;}
        for(;;){
            nanosleep(&debounce_timer, NULL);
            ret = gpiod_line_get_value_bulk(&btn, btn_vals_tmp);
            for (i = 0; i< COUNT_S; i++){
                if (btn_vals[i] != btn_vals_tmp[i]){
                    memcpy(&btn_vals, &btn_vals_tmp, sizeof(int)* COUNT_S);
                    continue;
                }
            }
            memcpy(&btn_vals, &btn_vals_tmp, sizeof(int)* COUNT_S);
            break;
        }
        printf("debounced\n");
        //we now have debounced values
	
    	gpiod_line_release_bulk(&btn);

    	ret = gpiod_line_request_bulk_falling_edge_events(&btn, "button-watch");

    	if (ret < 0) {
        	perror("Failed to request button lines\n");
        	goto free_led;
    	}
	

	for (i = 0; i < COUNT_S; i++){
		btn_vals[i] = btn_vals[i] == 0 ? 1 : 0;
	}
	
        k = 0;
        j = 0;
        printf("Read: ");
        for ( i = 0; i < COUNT_S; i++)
        {
            j += btn_vals[i];
            k += (i + 1) * btn_vals[i];
            printf(" %d", btn_vals[i]);
        }
        printf("\n");

        if (j == 0 ) {
            printf("no input\n");
            continue;
        }

        if (j != 1) {
            printf("resetting passwd\n");
            pos = 0;
            continue;
        }           

        printf("read %d\n", k);

        entered[pos] = k;
        pos++;
        if (pos < 4) {
            continue;
        }

        j=0;
        for(i=0; i<4; i++){
            if (passwd[i] != entered[i]){
                bad ++;
                pos = 0;
                j = 1;
                printf("Wrong\n");
                break;
            }
        }

        if (bad >=3) {
            printf("lock\n");

            for (i = 0; i < COUNT_L; i++){
                led_vals[i] = i%2;
            }

            gpiod_line_set_value_bulk(&led, led_vals);
            sleep(5);

            for (i = 0; i < COUNT_L; i++){
                led_vals[i] = 0;
            }

            gpiod_line_set_value_bulk(&led, led_vals);
            break;

        }

        if (j == 1) {
            continue;
        }

        printf("Correct!!\n");

        for (i = 0; i < COUNT_L; i++){
            led_vals[i] = 1;
        }

        gpiod_line_set_value_bulk(&led, led_vals);
        sleep(5);

        for (i = 0; i < COUNT_L; i++){
            led_vals[i] = 0;
        }

        gpiod_line_set_value_bulk(&led, led_vals);
        break;
    }



free_btn:
    gpiod_line_release_bulk(&btn);
free_led:
    gpiod_line_release_bulk(&led);
free_chip:
    gpiod_chip_close(chip); //void
end:
    return;
}
