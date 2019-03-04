#include "st7586.h"

/* Writer System - display driver for ST7586, source
 *
 * copyright Proportional Labs, 2018 - all rights reserved
 * confidential research software - not for release in any form
 *
 */
 
uint8_t* framebuffer;
spi_device_handle_t spi;

int display_raw(uint8_t data, uint8_t asel)
{
    esp_err_t ret;
    spi_transaction_t t;
    
    memset(&t, 0, sizeof(t));       
    t.length=8;                    
    t.tx_buffer=&data;             

    gpio_set_level(GPIO_DISPLAY_A0, (asel > 0) ? 1 : 0);
            
    ret=spi_device_transmit(spi, &t); 
    
    if(ret==ESP_OK) return 0;
    else return 1;
}

uint8_t* display_ptr()
{
    return framebuffer;
}

uint8_t* display_init()
{
    // Set up the SPI core
    esp_err_t ret;
    
    spi_bus_config_t buscfg={
        .miso_io_num=GPIO_DISPLAY_MISO,
        .mosi_io_num=GPIO_DISPLAY_MOSI,
        .sclk_io_num=GPIO_DISPLAY_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1
    };
    
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=10*1000*1000,               //Clock out at .1 MHz
        .mode=0,                                
        .spics_io_num=GPIO_DISPLAY_CS,               
        .queue_size=7,                      
    };
    
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    if(ret!=ESP_OK) return NULL;
   
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    if(ret!=ESP_OK) return NULL;
    
    // Set up other display pins
    gpio_set_direction(GPIO_DISPLAY_A0, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_DISPLAY_RST, GPIO_MODE_OUTPUT);
    
    // Make and clear framebuffer
    framebuffer = malloc(20480 * sizeof(uint8_t));
    memset(framebuffer, 0x00, 20480);
    
    // Reset display
    vTaskDelay(80 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_DISPLAY_RST, 0);
    vTaskDelay(80 / portTICK_PERIOD_MS);
    gpio_set_level(GPIO_DISPLAY_RST, 1);
    vTaskDelay(150 / portTICK_PERIOD_MS);
    
    // And then initialize
    display_raw(0x01, 0);
    display_raw(0x11, 0);
    display_raw(0x29, 0);
        
    // Initialize DDRAM
    display_raw(0x3A, 0);
    display_raw(0x02, 1);
    
    // Set scan direction
    display_raw(0x36, 0);
    display_raw(0xC0, 1);
    
    // Duty 
    display_raw(0xB0, 0);
    display_raw(0x9F, 1);
    display_raw(0xB1, 0);
    display_raw(0x00, 1);
    
    // Analog stuff (contrast)
    display_raw(0xC0, 0);
    display_raw(0xF6, 1);
    display_raw(0x00, 1);
    display_raw(0x30, 0);
    display_raw(0x00, 1);
    
    // Frame rate
    display_raw(0xF1, 0);
    display_raw(0x18, 1);
    display_raw(0x18, 1);
    display_raw(0x18, 1);
    display_raw(0x18, 1);
    
    dirtyMin = -1;
    dirtyMax = -1;
    
    return 0;
}

int display_halt()
{
    return 0;
}

int display_refresh(int debug)
{
    // "Stupid" refresh
    
    // Kill the display for a sec (if we want to)
    if(debug == 0) display_raw(0x28, 0);

    // We're gonna do this all in a go
    esp_err_t ret;
    spi_transaction_t t; 
    memset(&t, 0, sizeof(t)); 
    t.rxlength = 1;           
     
    gpio_set_level(GPIO_DISPLAY_A0, 0);
    
    uint8_t initData[14] = {0x2A, 0x00, 0x00, 0x00, 0x7F, 0x2B, 0x00, 0x00, 0x00, 0x9F, 0x37, 0x00, 0x38, 0x2C};
    t.length=14*8; 
    t.tx_buffer=initData;
    ret=spi_device_transmit(spi, &t); 
    
    gpio_set_level(GPIO_DISPLAY_A0, 1);
    
    t.length=4096; 
                        
    for(int y=0;y<40;y++)
    {
        t.tx_buffer=framebuffer+((y*4096)/8);             
        ret=spi_device_transmit(spi, &t); 
    }
    
    // And turn the display back on
    display_raw(0x29, 0);
    
    return 0;
}

int display_clear(int y1, int y2)
{   
    for(int r=y1;r<y2;r++)
    {
        memset(framebuffer+(128*r), 0x00, 8*sizeof(uint8_t));
        memset(framebuffer+(128*r)+8, 0x00, 120*sizeof(uint8_t));
    }
    
    return 0;
}

int display_blank()
{
    return display_clear(0, 160);
}

int display_image(uint8_t* data)
{
    // WARNING: this function naively assumes the given image is the right size.

    uint16_t i, j;
    for(i=0;i<160;i++)
    {
        for(j=0;j<8;j++)
            framebuffer[(128*i)+j] = 0xFF;
            
        for(j=0;j<60;j++)
        {
            uint8_t work = data[(60*i)+j], first = 0, second = 0;
            
            switch((work&0x30) >> 4)
            {
                case(0): first = 0x00; break;
                case(1): first = 0x40; break;
                case(2): first = 0x80; break;
                case(3): first = 0xE0; break;
            }
            
            switch((work&0xC0) >> 6)
            {
                case(0): first |= 0x00; break;
                case(1): first |= 0x08; break;
                case(2): first |= 0x10; break;
                case(3): first |= 0x1C; break;
            }
            
            switch((work&0x03) >> 0)
            {
                case(0): second = 0x00; break;
                case(1): second = 0x40; break;
                case(2): second = 0x80; break;
                case(3): second = 0xE0; break;
            }
            
            switch((work&0x0C) >> 2)
            {
                case(0): second |= 0x00; break;
                case(1): second |= 0x08; break;
                case(2): second |= 0x10; break;
                case(3): second |= 0x1C; break;
            }
            
            framebuffer[(128*i)+(8+(j*2))] = ~first;
            framebuffer[(128*i)+(8+(j*2)+1)] = ~second;
        }
    }
    
    dirtyMin = 0;
    dirtyMax = 159;
    
    return 0;
}

int display_draw_pixel(int x, int y, enum greycolor color)
{
    framebuffer[(128*y)+8+(int)(x/2)] &= ~(0x1C<<(3*(x%2)));
    framebuffer[(128*y)+8+(int)(x/2)] |= (color<<(3*(x%2)));
    return 0;
}

