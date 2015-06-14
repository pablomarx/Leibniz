#include "newton.h"
#include "monitor.h"
#include "runt.h"
#include <stdlib.h>

void FlushDisplay(const char *display, int width, int height) {
}

int main(int argc, char **argv) {
	char *romFile = (argc >= 2 ? argv[1] : "/Users/swhite/Desktop/Newton/ROMs/notepad-1.0b1.rom");	
	
	newton_t *newton = newton_new();
	if (newton_load_rom(newton, romFile) == -1) {
		return -1;
	}

	if (argc == 3) {
		newton_set_bootmode(newton, atoi(argv[2]));
	}
	
	runt_t *runt = newton_get_runt(newton);
    runt_set_log_flags(runt, RuntLogAll, 1);
	
	newton_set_break_on_unknown_memory(newton, true);

	monitor_t *monitor = monitor_new();
	monitor_set_newton(monitor, newton);
	monitor_run(monitor);
	
	return 0;
}
