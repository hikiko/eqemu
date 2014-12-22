/*
eqemu - electronic queue system emulator
Copyright (C) 2014  John Tsiombikas <nuclear@member.fsf.org>,
                    Eleni-Maria Stea <eleni@mutantstargoat.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include "dev.h"
#include "timer.h"

struct CustStat {
	int id;
	time_t start, end;
};

#define TMHIST_SIZE		16

void post_redisplay();	// defined in main.cc

int customer, ticket;
static int report_inputs, cmd_echo;
static long last_ticket_msec = LONG_MIN;

static std::vector<CustStat> cstat;

time_t calc_avg_wait();
static void runcmd(const char *cmd);

static int fd = -1;
static FILE *fp;
static std::string cur_line;

int start_dev(const char *devpath)
{
	if((fd = open(devpath, O_RDWR | O_NONBLOCK)) == -1) {
		fprintf(stderr, "failed to open device: %s: %s\n", devpath, strerror(errno));
		return -1;
	}
	if(isatty(fd)) {
		struct termios term;

		if(tcgetattr(fd, &term) == -1) {
			perror("failed to retrieve terminal attributes");
			stop_dev();
			return -1;
		}
		term.c_cflag = CS8 | CLOCAL;
		term.c_iflag &= ~(IXON | IXOFF);
		term.c_lflag = 0;

		cfsetispeed(&term, B38400);
		cfsetospeed(&term, B38400);

		if(tcsetattr(fd, TCSANOW, &term) == -1) {
			perror("failed to set terminal attributes");
			stop_dev();
			return -1;
		}
	}

	if(!(fp = fdopen(fd, "r+"))) {
		perror("failed to attach an I/O stream to the device file\n");
		stop_dev();
		return -1;
	}
	setvbuf(fp, 0, _IONBF, 0);

	return fd;
}

void stop_dev()
{
	if(fp)
		fclose(fp);
	else if(fd >= 0)
		close(fd);
}


void proc_dev_input()
{
	int rdbytes;
	char buf[256];
	static bool skip_line;

	while((rdbytes = read(fd, buf, sizeof buf - 1)) > 0) {
		buf[rdbytes] = 0;

		/* ignore our own crap */
		if(memcmp(buf, "OK,", 3) == 0 || memcmp(buf, "ERR,", 4) == 0) {
			skip_line = true;
		}

		for(int i=0; i<rdbytes; i++) {
			if(buf[i] == '\n' || buf[i] == '\r') {
				if(!cur_line.empty()) {
					runcmd(cur_line.c_str());
					cur_line.clear();
				}
				skip_line = false;
			} else {
				if(!skip_line) {
					cur_line.push_back(buf[i]);
				}
			}
		}
	}
}

void issue_ticket()
{
	ticket++;
	last_ticket_msec = get_msec();

	CustStat st;
	st.id = ticket;
	st.start = time(0);
	st.end = (time_t)-1;
	cstat.push_back(st);


	if(report_inputs) {
		fprintf(fp, "ticket: %d\n", ticket);
	}

	post_redisplay();
}

void next_customer()
{
	if(customer < ticket) {
		customer++;
		last_ticket_msec = LONG_MIN;

		for(size_t i=0; i<cstat.size(); i++) {
			if(cstat[i].id == customer) {
				cstat[i].end = time(0);
				fprintf(stderr, "start/end/interval: %lu %lu %lu\n", cstat[i].start,
						cstat[i].end, cstat[i].end - cstat[i].start);
				break;
			}
		}

		if(report_inputs) {
			fprintf(fp, "customer: %d\n", customer);
		}

		post_redisplay();
	}
}

time_t calc_avg_wait()
{
	int count = 0;
	time_t sum = 0;

	for(size_t i=0; i<cstat.size(); i++) {
		if(cstat[i].end != (time_t)-1) {
			sum += cstat[i].end - cstat[i].start;
			count++;
		}
	}

	return count ? sum / count : 0;
}

#define TICKET_SHOW_DUR		1000

int get_display_number()
{
	if(get_msec() - last_ticket_msec < TICKET_SHOW_DUR) {
		return ticket;
	}
	return customer;
}

int get_led_state(int led)
{
	int ledon = get_msec() - last_ticket_msec < TICKET_SHOW_DUR ? 0 : 1;
	return led == ledon ? 1 : 0;
}

#define VERSTR \
	"Queue system emulator v0.1"

static void runcmd(const char *cmd)
{
	printf("DBG: runcmd(\"%s\")\n", cmd);

	switch(cmd[0]) {
	case 'e':
		cmd_echo = !cmd_echo;
		fprintf(fp, "OK,turning echo %s\n", cmd_echo ? "on" : "off");
		break;

	case 'i':
		report_inputs = !report_inputs;
		fprintf(fp, "OK,turning input reports %s\n", report_inputs ? "on" : "off");
		break;

	case 'v':
		fprintf(fp, "OK,%s\n", VERSTR);
		break;

	case 'r':
		fprintf(fp, "OK,reseting queues\n");
		customer = 0;
		ticket = 0;
		last_ticket_msec = LONG_MIN;
		post_redisplay();
		break;

	case 't':
		fprintf(fp, "OK,ticket: %d\r\n", ticket);
		break;

	case 'c':
		fprintf(fp, "OK,customer: %d\r\n", customer);
		break;

	case 'q':
		fprintf(fp, "OK,issuing queue ticket\n");
		issue_ticket();
		break;

	case 'n':
		fprintf(fp, "OK,next customer\n");
		next_customer();
		break;

	case 'a':
		fprintf(fp, "OK,avg wait time: %lu\r\n", (unsigned long)calc_avg_wait());
		break;

	case 'h':
		fprintf(fp, "OK,commands: (e)cho, (v)ersion, (t)icket, (c)ustomer, "
				"(n)ext, (q)ueue, (a)verage wait time, (r)eset, (i)nput-reports, "
				"(h)elp.\n");
		break;

	default:
		fprintf(fp, "ERR,unknown command: %s\n", cmd);
	}
}
