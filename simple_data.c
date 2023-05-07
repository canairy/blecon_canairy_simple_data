/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include "pico/sem.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "stdio.h"
#include "string.h"

#include "RP2040.h" // CMSIS
#include "blecon_modem/blecon_modem.h"
#include "blecon_modem_pico/blecon_modem_pico_spi_transport.h"

// Utility macro
#define BLECON_ERROR_CHECK(blecon_call)                    \
    do {                                                   \
        enum blecon_modem_error_code_t code = blecon_call; \
        if (code != blecon_modem_ok) {                     \
            printf("Blecon modem error: %x", code);        \
        };                                                 \
    } while (0)

// Use the following SPI port and pins
#define SPI_PORT spi0
#define PIN_COPI 3
#define PIN_CIPO 4
#define PIN_CS 5
#define PIN_SCK 2
#define PIN_IRQ 6

static void blecon_modem_on_connection(struct blecon_modem_t* modem, void* user_data);
static void blecon_modem_on_response(struct blecon_modem_t* modem, void* user_data);
static void blecon_modem_on_error(struct blecon_modem_t* modem, void* user_data);
static void blecon_modem_on_event_irq(struct blecon_modem_t* modem, void* user_data);

const static struct blecon_modem_callbacks_t blecon_modem_callbacks = {
    .on_connection = blecon_modem_on_connection,
    .on_response = blecon_modem_on_response,
    .on_error = blecon_modem_on_error,
    .on_event_irq = blecon_modem_on_event_irq
};

static semaphore_t blecon_event_sem;
static struct blecon_modem_t blecon_modem;
static struct blecon_modem_pico_spi_transport_t blecon_modem_pico_spi_transport;
static char blecon_buffer[1024] = { 0 };

static repeating_timer_t data_timer;
static unsigned int msg_count;

int send_msg(void)
{
    printf("Sending request #%u\n", msg_count);

    // Create message
    sprintf(blecon_buffer, "Message count: %u", msg_count);

    // Send request
    BLECON_ERROR_CHECK(blecon_modem_send_request(&blecon_modem, (const uint8_t*)blecon_buffer, strlen(blecon_buffer)));

    return 0;
}

bool data_timer_callback(repeating_timer_t* p_rt)
{
    msg_count++;
    send_msg();
}

int main()
{
    stdio_init_all();
    msg_count = 0;

    // Initialise SPI bus and pins
    blecon_modem_pico_spi_transport_bus_init(SPI_PORT, PIN_COPI, PIN_CIPO, PIN_SCK);

    // Give a chance to a terminal to connect
    sleep_ms(3000);

    // Initialise event IRQ semaphore
    sem_init(&blecon_event_sem, 1, 1);

    // Initialise SPI transport
    blecon_modem_pico_spi_transport_init(&blecon_modem_pico_spi_transport, SPI_PORT, PIN_CS, PIN_IRQ);

    // Initialise modem
    BLECON_ERROR_CHECK(blecon_modem_init(&blecon_modem,
        blecon_modem_pico_spi_transport_as_transport(&blecon_modem_pico_spi_transport),
        &blecon_modem_callbacks,
        NULL));

    // Retrieve device URL
    BLECON_ERROR_CHECK(blecon_modem_get_device_url(&blecon_modem, blecon_buffer, sizeof(blecon_buffer)));

    printf("Blecon URL: %s\n", blecon_buffer);

    // Request connection
    BLECON_ERROR_CHECK(blecon_modem_request_connection(&blecon_modem));

    while (true) {
        // Wait for semaphore
        sem_acquire_blocking(&blecon_event_sem);

        // Process event
        BLECON_ERROR_CHECK(blecon_modem_process_event(&blecon_modem));
    }
    return 0;
}

void blecon_modem_on_connection(struct blecon_modem_t* modem, void* user_data)
{
    printf("Connected, sending request\n");

    // Create message
    sprintf(blecon_buffer, "Hello Blecon!");

    // Send request
    BLECON_ERROR_CHECK(blecon_modem_send_request(&blecon_modem, (const uint8_t*)blecon_buffer, strlen(blecon_buffer)));

    // Start sending data every three seconds
    bool timer_added = add_repeating_timer_ms(3000, data_timer_callback, NULL, &data_timer);
    if (timer_added == false) {
        printf("ERROR: Could not add data timer!\n");
    }
}

void blecon_modem_on_response(struct blecon_modem_t* modem, void* user_data)
{
    printf("Got response:\n");

    // Read response
    size_t message_sz = sizeof(blecon_buffer) - 1; // Reserve space for 0 terminator
    BLECON_ERROR_CHECK(blecon_modem_get_response(&blecon_modem, (uint8_t*)blecon_buffer, &message_sz));
    blecon_buffer[message_sz] = '\0';

    // Display response
    printf("%s\n", blecon_buffer);

    // Close connection
    BLECON_ERROR_CHECK(blecon_modem_close_connection(&blecon_modem));
}

void blecon_modem_on_error(struct blecon_modem_t* modem, void* user_data)
{
    enum blecon_modem_rpc_error_t error = 0;
    BLECON_ERROR_CHECK(blecon_modem_get_error(&blecon_modem, &error));
    printf("Got error: %u\n", error);

    // Close connection
    BLECON_ERROR_CHECK(blecon_modem_close_connection(&blecon_modem));
}

void blecon_modem_on_event_irq(struct blecon_modem_t* modem, void* user_data)
{
    // Release semaphore
    sem_release(&blecon_event_sem);
}
