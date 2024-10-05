#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

#define MAXINPUT 256
#define MAX_RESULTS 10

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 380
#define BACKGROUND_COLOR "#800080"
#define INPUT_TEXT_COLOR "#40e0d0"
#define RESULT_TEXT_COLOR "#FF98F1"


void createRoundedRectMask(Pixmap *mask, Display *dpy, int width, int height, int radius) {
  *mask = XCreatePixmap(dpy, DefaultRootWindow(dpy), width, height, 1);
  GC gc = XCreateGC(dpy, *mask, 0, NULL);
  XSetForeground(dpy, gc, 0);
  XFillRectangle(dpy, *mask, gc, 0, 0, width, height);
  XSetForeground(dpy, gc, 1);
  XFillArc(dpy, *mask, gc, 0, 0, radius * 2, radius * 2, 90 * 64, 90 * 64);
  XFillArc(dpy, *mask, gc, width - radius * 2, 0, radius * 2, radius * 2, 0 * 64, 90 * 64);
  XFillArc(dpy, *mask, gc, 0, height - radius * 2, radius * 2, radius * 2, 180 * 64, 90 * 64);
  XFillArc(dpy, *mask, gc, width - radius * 2, height - radius * 2, radius * 2, radius * 2, 270 * 64, 90 * 64);
  XFillRectangle(dpy, *mask, gc, radius, 0, width - radius * 2, height);
  XFillRectangle(dpy, *mask, gc, radius, height - radius, width - radius * 2, radius);
  XFillRectangle(dpy, *mask, gc, 0, radius, width, height - radius * 2);
  XFreeGC(dpy, gc);
}

void launchApplication(const char *app) {
  pid_t pid = fork();
  if (pid == 0) {
    execlp(app, app, (char *) NULL);
    perror("execlp failed");
    exit(EXIT_FAILURE);
  }
}

void drawInput(Display *dpy, Window win, XftDraw *draw, XftFont *font, const char *input, char results[][MAXINPUT], int count) {
  XftColor bg_color, input_color, result_color;
  XftColorAllocName(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), BACKGROUND_COLOR, &bg_color);
  XftColorAllocName(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), INPUT_TEXT_COLOR, &input_color);
  XftColorAllocName(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), RESULT_TEXT_COLOR, &result_color);
  XClearWindow(dpy, win);
  XftDrawRect(draw, &bg_color, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  XftDrawStringUtf8(draw, &input_color, font, 10, 30, (const XftChar8 *)input, strlen(input));
  for (int i = 0; i < count; i++) {
    int y_position = 80 + i * 30;
    XftDrawStringUtf8(draw, &result_color, font, 10, y_position, (const XftChar8 *)results[i], strlen(results[i]));
  }
  XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), &bg_color);
  XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), &input_color);
  XftColorFree(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), DefaultColormap(dpy, DefaultScreen(dpy)), &result_color);
}

void findMatchingApplications(const char *query, char results[][MAXINPUT], int *count) {
  const char *paths[] = {"/usr/local/bin", "/bin"};
  *count = 0;

  for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
    DIR *dir = opendir(paths[i]);
    if (!dir) {
      perror("Failed to open directory");
      continue;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      if (*count >= MAX_RESULTS) break;

      if (strncmp(entry->d_name, query, strlen(query)) == 0) {
        if (strlen(entry->d_name) < MAXINPUT) {
          strncpy(results[*count], entry->d_name, MAXINPUT - 1);
          results[*count][MAXINPUT - 1] = '\0'; 
          (*count)++;
        }
      }
    }
    closedir(dir);
  }

  for (int i = 0; i < *count - 1; i++) {
    for (int j = 0; j < *count - i - 1; j++) {
      if (strlen(results[j]) > strlen(results[j + 1])) {
        char temp[MAXINPUT];
        strncpy(temp, results[j], MAXINPUT);
        strncpy(results[j], results[j + 1], MAXINPUT);
        strncpy(results[j + 1], temp, MAXINPUT);
      }
    }
  }
}

void completeInputWithFirstResult(char *input, char results[][MAXINPUT], int count) {
  if (count > 0) {
    strncpy(input, results[0], MAXINPUT - 1);
    input[MAXINPUT - 1] = '\0';
  }
}

int main() {
  Display *dpy = XOpenDisplay(NULL);
  if (!dpy) {
    fprintf(stderr, "Unable to open display\n");
    return 1;
  }

  int screen = DefaultScreen(dpy);
  Window root = RootWindow(dpy, screen);
  Window win = XCreateSimpleWindow(dpy, root, 100, 100, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
      BlackPixel(dpy, screen), WhitePixel(dpy, screen));

  XSetWindowAttributes attrs;
  attrs.override_redirect = False;
  XChangeWindowAttributes(dpy, win, CWOverrideRedirect, &attrs);

  XClassHint *classHint = XAllocClassHint();
  classHint->res_name = "emenu";
  classHint->res_class = "emenu";
  XSetClassHint(dpy, win, classHint);
  XFree(classHint);

  Pixmap mask;
  createRoundedRectMask(&mask, dpy, WINDOW_WIDTH, WINDOW_HEIGHT, 20);
  XShapeCombineMask(dpy, win, ShapeBounding, 0, 0, mask, ShapeSet);
  XFreePixmap(dpy, mask);

  char input[MAXINPUT] = "";
  char results[MAX_RESULTS][MAXINPUT] = {0};
  int resultCount = 0;

  XSelectInput(dpy, win, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
  XMapWindow(dpy, win);

  XftDraw *draw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
  XftFont *font = XftFontOpenName(dpy, screen, "Cantarell:style=Regular:size=20");
  if (!font) {
    fprintf(stderr, "Failed to load font\n");
    return 1;
  }

  XEvent event;
  while (1) {
    XNextEvent(dpy, &event);
    if (event.type == DestroyNotify)
      break;

    if (event.type == Expose) {
      drawInput(dpy, win, draw, font, input, results, resultCount);
    }

    if (event.type == KeyPress) {
      if (event.xkey.keycode == 9) {
        break;
      } else if (event.xkey.keycode == 36) {
        if (resultCount > 0) {
          launchApplication(results[0]);
        }
        memset(input, 0, MAXINPUT);
        resultCount = 0;
        break;
      } else if (event.xkey.keycode == 23) {
        completeInputWithFirstResult(input, results, resultCount);
      } else {
        char buffer[32];
        KeySym keysym;
        XLookupString(&event.xkey, buffer, sizeof(buffer), &keysym, NULL);

        if (keysym == XK_BackSpace) {
          if (strlen(input) > 0) {
            input[strlen(input) - 1] = '\0';
          }
        } else if (strlen(input) < MAXINPUT - 1) {
          strncat(input, buffer, 1);
        }

        findMatchingApplications(input, results, &resultCount);
      }
      drawInput(dpy, win, draw, font, input, results, resultCount);
    }
  }

  XftDrawDestroy(draw);
  XftFontClose(dpy, font);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
  return 0;
}
