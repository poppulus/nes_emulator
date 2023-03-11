#include <GL/glew.h>
#include <GL/glut.h>
#include "audio.h"
#include "emu.h"

Frame frame;
CPU cpu;

unsigned int texture;

bool key_states[256];   //key_special_states[256]
bool quit;

static void close_window()
{
    int err = Pa_Terminate();

    if (err != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));

    rom_reset(&cpu.bus.rom);
    glDeleteTextures(1, &texture);

    glutDestroyWindow(glutGetWindow());

    printf("clean exit\n");
}

static void key_down(unsigned char key, int x, int y)
{
    key_states[key] = true;
}

static void key_up(unsigned char key, int x, int y)
{
    key_states[key] = false;
}

static void key_special_down(unsigned char key, int x, int y)
{
    
}

static void key_special_up(unsigned char key, int x, int y)
{

}

static void key_handler()
{
    if (key_states[0x1B])    
    {
        quit = true;
        return;
    }

    if (key_states['.'])    cpu.bus.joypad1.button_status |= A;
    else                    cpu.bus.joypad1.button_status &= 0b11111110;

    if (key_states[','])    cpu.bus.joypad1.button_status |= B;
    else                    cpu.bus.joypad1.button_status &= 0b11111101;

    if (key_states[8])      cpu.bus.joypad1.button_status |= SELECT;
    else                    cpu.bus.joypad1.button_status &= 0b11111011;

    if (key_states[13])     cpu.bus.joypad1.button_status |= START;
    else                    cpu.bus.joypad1.button_status &= 0b11110111;

    if (key_states['w'])    cpu.bus.joypad1.button_status |= UP;
    else                    cpu.bus.joypad1.button_status &= 0b11101111;

    if (key_states['s'])    cpu.bus.joypad1.button_status |= DOWN;
    else                    cpu.bus.joypad1.button_status &= 0b11011111;

    if (key_states['a'])    cpu.bus.joypad1.button_status |= LEFT;
    else                    cpu.bus.joypad1.button_status &= 0b10111111;

    if (key_states['d'])    cpu.bus.joypad1.button_status |= RIGHT;
    else                    cpu.bus.joypad1.button_status &= 0b01111111;
}

static void render()
{
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGB, GL_UNSIGNED_BYTE, frame.data);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, 1.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(+1.0f, 1.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(+1.0f, -1.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, -1.0f);

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    glutSwapBuffers();
    glFlush();
}

static void emu_loop()
{
    if (quit) 
    {
        close_window();
        return;
    }
    
    cpu_interpret(&cpu);
}

void cpu_callback(Bus *bus)
{
    if (bus->ppu.mask & BACKGROUND_SHOW 
    && bus->ppu.mask & SPRITES_SHOW)
    {
        ppu_render(&bus->ppu, &frame);
        render();
    }
    
    key_handler();
}

static void emu_init(CPU *cpu, Frame *frame)
{
    FILE *f;

    f = fopen("super.nes", "r");

    if (!f)
    {
        printf("Could not open file!\n");
        return;
    }

    unsigned char *file_buffer;
    int i = 0;

    file_buffer = malloc(40976);

    while (!feof(f))
    {
        file_buffer[i] = fgetc(f);
        i++;
    }

    fclose(f);

    rom_init(&cpu->bus.rom);

    if (!rom_load(&cpu->bus.rom, file_buffer))
    {
        printf("could not load rom!\n");
        free(file_buffer);
        return;
    }

    free(file_buffer);

    cpu_init(cpu);
    joypad_init(&cpu->bus.joypad1);
    ppu_load(&cpu->bus.ppu, cpu->bus.rom.chr_rom, cpu->bus.rom.screen_mirroring);
    addr_reset(&cpu->bus.ppu.addr);

    frame_init(frame);
}

void reshape(int width, int height)
{
    
}

int main(int argc, char *argv[])
{
    glutInit(&argc, (char**)argv);
    glutInitDisplayMode(GLUT_SINGLE);
    glutInitWindowSize(256, 240);
    glutInitWindowPosition(832, 420);
    glutCreateWindow("NES Emulator");

    // setup texture
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 240, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.data);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glutReshapeFunc(reshape);

    glutDisplayFunc(render);
    glutIdleFunc(emu_loop);

    glutKeyboardFunc(key_down);
    glutKeyboardUpFunc(key_up);

    int err = Pa_Initialize();

    if (err != paNoError)
    {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        goto quit;
    }

    memset(key_states, false, 256);

    emu_init(&cpu, &frame);

    quit = false;

    glutMainLoop();

quit:

    err = Pa_Terminate();

    if (err != paNoError)
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));

    rom_reset(&cpu.bus.rom);
    glDeleteTextures(1, &texture);

    printf("clean exit\n");
    return 0;
}
