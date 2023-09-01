
#include <input_manager.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>

static int StdinDevInit(void)
{
	struct termios tTTYState;

	// get the terminal state
	tcgetattr(STDIN_FILENO, &tTTYState);

	// turn off canonical mode
	tTTYState.c_lflag &= ~ICANON;
	// minimum of number input read.
	tTTYState.c_cc[VMIN] = 1;

	// set the terminal attributes.
	tcsetattr(STDIN_FILENO, TCSANOW, &tTTYState);

	return 0;
}

static int StdinDevExit(void)
{

	struct termios tTTYState;

	// get the terminal state
	tcgetattr(STDIN_FILENO, &tTTYState);

	// turn on canonical mode
	tTTYState.c_lflag |= ICANON;

	// set the terminal attributes.
	tcsetattr(STDIN_FILENO, TCSANOW, &tTTYState);
	return 0;
}

static int StdinGetInputEvent(PT_InputEvent ptInputEvent)
{

	char c;

	ptInputEvent->iType = INPUT_TYPE_STDIN;

	c = fgetc(stdin);
	gettimeofday(&ptInputEvent->tTime, NULL);

	return 0;
}

static T_InputOpr g_tStdinOpr = {
		.name = "stdin",
		.DeviceInit = StdinDevInit,
		.DeviceExit = StdinDevExit,
		.GetInputEvent = StdinGetInputEvent,
};

int StdinInit(void)
{
	return RegisterInputOpr(&g_tStdinOpr);
}
