// Bibliotecas que seão usadas no programa
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pio_matrix.pio.h"
#include "pico/bootrom.h"
// Fim includes

// Macros que são usados no programa
#define I2C_PORT i2c1 // Porta I2C
#define I2C_SDA 14 // Define o pino 14 para a linha de dados (SDA)
#define I2C_SCL 15 // Define o pino 15 para a linha de clock (SCL)
#define endereco 0x3C // Endereço I2C
#define NUM_LEDS 25 // Número de LEDs na matriz
#define OUT_PIN 7 // Pino de dados conectado à matriz
#define PIN_BUTTON_A 5 // Pino para o botão A
#define PIN_BUTTON_B 6 // Pino para o botão B
#define LED_PIN_GREEN 11 // Pino para o led verde
#define LED_PIN_RED 13 // Pino para o led Vermelho
// Fim macros

// Matrizes para a animação na matriz de leds
double drawing1[25] = {
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0
};

double drawing2[25] = {
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0, 
    0.0, 0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0
};

double drawing3[25] = {
    0.0, 0.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 1.0, 1.0, 0.0, 
    0.0, 1.0, 0.0, 1.0, 0.0,
    0.0, 1.0, 1.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 0.0, 0.0
};

double drawing4[25] = {
    1.0, 1.0, 1.0, 1.0, 1.0,
    1.0, 0.0, 0.0, 0.0, 1.0, 
    1.0, 0.0, 0.0, 0.0, 1.0,
    1.0, 0.0, 0.0, 0.0, 1.0,
    1.0, 1.0, 1.0, 1.0, 1.0
};
// Fim matrizes

// Variaveis globais usadas no programa
static volatile uint32_t LAST_TIME_A = 0; // Variavel para pegar o ultimo tempo marcado para o botão A
static volatile uint32_t LAST_TIME_B = 0; // Variavel para pegar o ultimo tempo marcado para o botão B
static volatile bool on_off = true; // Variavel para ligar ou desligar a matriz de leds
static volatile uint time_animation = 0; // Variável para guardar qual led deve ser ligado
static volatile uint time = 200; // Tempo entre um frame e outro
// Fim variaveis

// Declaração das funções criadas para o funionamento do programa
float get_cpu_temperature();
uint get_clock();
uint32_t matrix_rgb(float r, float g, float b);
void draw_animation(double *drawing, uint32_t led_value, PIO pio, uint sm, double r, double g, double b);
void gpio_irq_handler(uint gpio, uint32_t events);
void initialize_led(uint led);
void initialize_button(uint pin);
void initialize_i2c();
void initialize_display(ssd1306_t* ssd);
// Fim declaração de funções

// Função principal
int main() {
    stdio_init_all(); // Inicializa a entrada e saida padrão

    // Variaveis para conffiguração do PIO, Matriz de Leds e Display
    PIO pio = pio0;
    bool ok;
    uint16_t i;
    uint32_t valor_led;
    double r = 0.0, b = 0.0, g = 0.0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &pio_matrix_program);
    ok = set_sys_clock_khz(128000, false);
    bool cor = true; // Controla o disolay
    int count = 0; // Contador para impressões no display
    int j; // Contador para a matriz de led

    pio_matrix_program_init(pio, sm, offset, OUT_PIN); // Inicializa o PIO
    adc_init(); // Inicializa o ADC
    adc_set_temp_sensor_enabled(true); // Seta o sensor como habilitado

    initialize_led(LED_PIN_GREEN); // Inicializa o pino do led verde
    initialize_led(LED_PIN_RED); // Inicializa o pino do led vermelho

    initialize_button(PIN_BUTTON_A); // Inicializa o pino do botão A
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Configura a interrupção do botão A

    initialize_button(PIN_BUTTON_B); // Inicializa o pino do botão B
    gpio_set_irq_enabled_with_callback(PIN_BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); // Configura a interrupção do botão B

    initialize_i2c(); // Inicializa o I2C

    // Configura o display
    ssd1306_t ssd;
    initialize_display(&ssd);

    while (true) {
        cor = !cor;

        if(count == 0) {
            ssd1306_fill(&ssd, false); // Limpa a tela do display
            ssd1306_send_data(&ssd); // Atualiza o display

            // Imprime as informações no display
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
            ssd1306_draw_string(&ssd, "Monitoramento", 10, 10);
            ssd1306_draw_string(&ssd, "Do", 50, 30);
            ssd1306_draw_string(&ssd, "Sistema", 30, 50);
            ssd1306_send_data(&ssd);

            count++; // Incrementa a variavel count
        } else if(count == 1) { // Caso sejá 1, a temperatura vai ser impressa na tela do display
            float temperature = get_cpu_temperature(); // Pega a temmperatura
            char temperatura_info[10]; // Variavel para guardar a temperatura em forma de string
            sprintf(temperatura_info, "%.2f", temperature); // Converte a temperatura de float para string
            strcat(temperatura_info, " C"); // Concatena o "C" no final da temperatura

            if(temperature > 85.0) {
                gpio_put(LED_PIN_GREEN, false);
                gpio_put(LED_PIN_RED, true);

                ssd1306_fill(&ssd, false); // Limpa a tela do display
                ssd1306_send_data(&ssd); // Atualiza o display

                // Imprime as informações no display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                ssd1306_draw_string(&ssd, "Temperatura", 20, 10);
                ssd1306_draw_string(&ssd, "Muito Alta", 30, 30);
                ssd1306_send_data(&ssd);
            } else {
                gpio_put(LED_PIN_RED, false);
                gpio_put(LED_PIN_GREEN, true);

                ssd1306_fill(&ssd, false); // Limpa a tela do display
                ssd1306_send_data(&ssd); // Atualiza o display

                // Imprime as informações no display
                ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
                ssd1306_draw_string(&ssd, "Temperatura", 20, 10);
                ssd1306_draw_string(&ssd, temperatura_info, 30, 30);
                ssd1306_send_data(&ssd);
            }

            count++; // Incrementa a variavel count
        } else if(count == 2) { // Caso seja 2, vai mostrar o clock da placa
            uint32_t clock = get_clock(); // Obtem o clock
            char clock_info[50]; // Variavel que vai guardar o valor do clock como string
            sprintf(clock_info, "%u", clock); // Converte o clock para uma string
            strcat(clock_info, " Hz"); // Concatena "KHz" no final do valor do clock

            ssd1306_fill(&ssd, false); // Limpa a tela do display
            ssd1306_send_data(&ssd); // Atualiza o display

            // Imprime as informações na tela do display
            ssd1306_rect(&ssd, 3, 3, 122, 58, cor, !cor);
            ssd1306_draw_string(&ssd, "Clock", 50, 10);
            ssd1306_draw_string(&ssd, clock_info, 20, 30);
            ssd1306_send_data(&ssd);

            count = 0; // Count volta a ser zero para imprimir "Monitoramento De Componentes" no display
        }

        // Mostra uma animação na matriz de leds
        for(j = 0; j < 10; j++) {
            if(on_off) { // Verifica se a matriz deve estar liga e mostra a animação
                draw_animation(drawing2, valor_led, pio, sm, r, g, b);
                sleep_ms(time);
                draw_animation(drawing3, valor_led, pio, sm, r, g, b);
                sleep_ms(time);
                draw_animation(drawing4, valor_led, pio, sm, r, g, b);
                sleep_ms(time);
                draw_animation(drawing1, valor_led, pio, sm, r, g, b);
                sleep_ms(time);
            } else { // Caso estteja desligada e apaga a matriz de leds
                draw_animation(drawing1, valor_led, pio, sm, r, g, b);
                sleep_ms(time * 4);
            }
        }
    }

    return 0;
} // Fim main

// Função para obter a temperatura da placa
float get_cpu_temperature() {
    adc_select_input(4);
    uint16_t raw_value = adc_read();
    float voltage = raw_value * 3.3f / (1 << 12);
    float temperature = 27.0f - (voltage - 0.706f) / 0.001721f;
    return temperature;
} // Fim get_cpu_temperature

// Função para obter o clock da placa
uint get_clock() {
    uint clock_value = clock_get_hz(clk_sys); // Obtém o clock atual da CPU
    return clock_value;
} // Fim get_clock

// Função para fazer animação na matriz de leds
void draw_animation(double *drawing, uint32_t led_value, PIO pio, uint sm, double r, double g, double b){
    int16_t i;

    for (i = 0; i < NUM_LEDS; i++) {
        if (i % 2 == 0) {
            led_value = matrix_rgb(drawing[24 - i], r = 0.0, g = 0.0);
            pio_sm_put_blocking(pio, sm, led_value);

        } else {
            led_value = matrix_rgb(b = 0.0, drawing[24 - i], g = 0.0);
            pio_sm_put_blocking(pio, sm, led_value);
        }
    }
} //Fim draw_animation

// Função para tratar as cores da matriz de leds
uint32_t matrix_rgb(float r, float g, float b) {
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;

    return (G << 24) | (R << 16) | (B << 8);
} // Fim matrix_rgb

// Função para tratar as interrupções dos botões A e B
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t current_time = to_us_since_boot(get_absolute_time());

    if(gpio == PIN_BUTTON_A && current_time - LAST_TIME_A > 200000) {
        LAST_TIME_A = current_time;
    
        if(time_animation == 0) {
            time = time - 100;

            time_animation++;
        } else {
            time = time + 100;

            time_animation--;
        }
    } else if(gpio == PIN_BUTTON_B && current_time - LAST_TIME_B > 200000) { // Liga e desliga a matriz de leds
        LAST_TIME_B = current_time;

        on_off = !on_off;
    }
} // Fim gpio_irq_handler

// Função para inicializar os leds
void initialize_led(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
} // Fim initialize_led

// Função para inicializar os botões
void initialize_button(uint pin) {
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
} // Fim initialize_button

// Função para inicializar o I2C
void initialize_i2c() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
} // initialize_i2c

// Função para configurar o display
void initialize_display(ssd1306_t* ssd) {
    ssd1306_init(ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT);
    ssd1306_config(ssd);
    ssd1306_send_data(ssd);
} // Fim initialize_display
