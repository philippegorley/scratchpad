#include <GL/glut.h>
#include <stdio.h>

// gcc gl_info.c -lGL -lGLU -lglut

int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutCreateWindow("Test");
    glutHideWindow();

    printf("Vendor     %s\n", glGetString(GL_VENDOR));
    printf("Version    %s\n", glGetString(GL_VERSION));
    printf("Renderer   %s\n", glGetString(GL_RENDERER));
}
