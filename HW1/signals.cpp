#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
	cout << "smash: got ctrl-Z" << endl;
	SmallShell& smash = SmallShell::getInstance();
	JobsList::JobEntry* fg_job = smash.getJobsList()->getFgJob();
	if (fg_job != nullptr) {
		pid_t fg_job_pid = fg_job->getJobPid();
		if (kill(fg_job_pid, SIGSTOP) == -1) {
			perror("smash error: kill failed");
		} else {
			smash.getJobsList()->addJob(fg_job->getCmdLine(), fg_job_pid, fg_job->getJobId());
			smash.getJobsList()->getJobByPid(fg_job_pid)->setJobStatus(JobsList::JobStatus::Stopped);
			smash.getJobsList()->setFgJob(nullptr);
			cout << "smash: process " << fg_job_pid << " was stopped" << endl;
		}
	}
}

void ctrlCHandler(int sig_num) {
	cout << "smash: got ctrl-C" << endl;
	SmallShell& smash = SmallShell::getInstance();
	JobsList::JobEntry* fg_job = smash.getJobsList()->getFgJob();
	if (fg_job != nullptr) {
		pid_t fg_job_pid = fg_job->getJobPid();
		if (kill(fg_job_pid, SIGKILL) == -1) {
			perror("smash error: kill failed");
		} else {
			smash.getJobsList()->setFgJob(nullptr);
			cout << "smash: process " << fg_job_pid << " was killed" << endl;
		}
	}
}
