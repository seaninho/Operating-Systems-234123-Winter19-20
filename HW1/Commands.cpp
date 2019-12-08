#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <linux/limits.h>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

const string WHITESPACE = " \n\r\t\f\v";

string _ltrim(const string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == string::npos) ? "" : s.substr(start);
}

string _rtrim(const string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
    FUNC_ENTRY()

    int i = 0;
    istringstream iss(_trim(string(cmd_line)).c_str());
    for (string s; iss >> s;) {
        args[i] = (char*) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = nullptr;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundCommand(const char* cmd_line) {
    const string whitespace = " \t\n";
    const string str(cmd_line);
    return str[str.find_last_not_of(whitespace)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
    const string whitespace = " \t\n";
    const string str(cmd_line);
    // find last character other than spaces
    size_t idx = str.find_last_not_of(whitespace);
    // if all characters are spaces then return
    if (idx == string::npos) {
        return;
    }
    // if the command line does not end with & then return
    if (cmd_line[idx] != '&') {
        return;
    }
    // replace the & (background sign) with space and then remove all tailing spaces.
    cmd_line[idx] = ' ';
    // truncate the command line string up to the last non-space character
    cmd_line[str.find_last_not_of(whitespace, idx - 1) + 1] = 0;
}


void _printError(string error) {
    cout << "smash error: " << error << endl;
}

string _getCWD(){
	char cwd[PATH_MAX];
	if (!getcwd(cwd, sizeof(cwd))) {
		perror("smash error: getcwd failed");
		return "";
	} else {
		return string(cwd);
	}
}

Command::Command(const char* cmd_line) {
	this->cmd = cmd_line;
	this->args_num = _parseCommandLine(cmd_line, this->args);
}

void ExternalCommand::execute() {
	bool is_bg = false;
	char new_cmd[COMMAND_ARGS_MAX_LENGTH];

	if (_isBackgroundCommand(cmd)) {
		is_bg = true;
		strcpy(new_cmd, cmd);
		_removeBackgroundSign(new_cmd);
	}

	pid_t pid = fork();
	if (pid == 0) {
		// child process
		setpgrp();
		if (_isBackgroundCommand(cmd)) {
			execl(BASH_PATH, "bash", "-c", new_cmd, nullptr);
		} else {
			execl(BASH_PATH, "bash", "-c", cmd, nullptr);
		}
		exit(-1);
	} else if (pid > 0) {
		// father process
		if (is_bg) {
			this->jobs->addJob(cmd, pid);
			return;
		} else {
			// adding new fg_job
			auto* fg_job = new JobsList::JobEntry(cmd, 0, pid, time(nullptr));
			jobs->setFgJob(fg_job);
			int status = 0;
			int val = waitpid(pid, &status, WUNTRACED);
			if (val == -1) {
				perror("smash error: waitpid failed");
				return;
			} else if (val == pid) {
				if (WIFSTOPPED(status)) {
					jobs->getJobByPid(pid)->setJobStatus(JobsList::JobStatus::Stopped);
					jobs->setFgJob(nullptr);
				}
			}
		}
	} else {
		// fork error
		perror("smash error: fork failed");
		return;
	}
}

void ChangeDirCommand::execute() {
	char* path = this->args[1];
	if (this->args_num > 2) {
		_printError("cd: too many arguments");
		return;
	} else if ((strcmp(path, "-") == 0) && SmallShell::getInstance().getLastWd().empty()) {
		_printError("cd: OLDPWD not set");
		return;
	} else {
		string curr_wd = _getCWD();
		string last_wd = SmallShell::getInstance().getLastWd();
		if (strcmp(path, "-") == 0) {
			if (chdir(last_wd.c_str()) != 0) {
				perror("smash error: chdir failed");
				return;
			} else {
				SmallShell::getInstance().setLastWd(curr_wd);
				SmallShell::getInstance().setCurrWd(last_wd);
			}
		} else {
			if (chdir(path) != 0) {
				perror("smash error: chdir failed");
				return;
			} else {
				SmallShell::getInstance().setLastWd(curr_wd);
				SmallShell::getInstance().setCurrWd(path);
			}
		}
	}
}

void GetCurrDirCommand::execute() {
	cout << _getCWD() << endl;
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    pid = to_string(getpid());
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << pid << endl;
}

void QuitCommand::execute() {
	if (args_num > 1 && string(args[1]) == "kill") {
		jobs->removeFinishedJobs();
		cout << "smash: sending SIGKILL signal to " << jobs->getJobsListSize() << " jobs:" << endl;
		jobs->killAllJobs();
	}
	exit(0);
}

CommandsHistory::CommandsHistory() {
//	next_entry_idx = 0;
	last_entry_idx = -1;
	history_seq_num = 0;
}

void CommandsHistory::addRecord(const char* cmd_line) {
    history_seq_num++;
    if (!history.empty()) {
    	if (history[last_entry_idx].getCmdLine() == string(cmd_line)) {
		    history[last_entry_idx].setSeqNum(history_seq_num);
		    return;
	    }
    }

    CommandHistoryEntry new_entry = CommandHistoryEntry(cmd_line, history_seq_num);
	if (history.size() == HISTORY_MAX_RECORDS) {
		last_entry_idx = (last_entry_idx + 1) % HISTORY_MAX_RECORDS;
		history[last_entry_idx] = new_entry;
	} else {
		last_entry_idx++;
		history.push_back(new_entry);
	}
}

void CommandsHistory::printHistory() {
	// sorting commands history list according to seq_num
	vector<CommandHistoryEntry> history_for_print(history.begin(), history.end());
	sort(history_for_print.begin(), history_for_print.end());

    vector<CommandHistoryEntry>::iterator entry;
    for (entry = history_for_print.begin(); entry != history_for_print.end(); ++entry) {
        cout << right << setw(5) << entry->getEntrySeqNum() << "  " << entry->getCmdLine() << endl;
    }
}

HistoryCommand::HistoryCommand(const char* cmd_line, CommandsHistory* history) : BuiltInCommand(cmd_line) {
	this->history = history;
}

void HistoryCommand::execute() {
	this->history->printHistory();
}

JobsList::JobEntry::JobEntry(string cmd_line, int jobId, pid_t pid, time_t added_time)
	: cmd_line(cmd_line), jid(jobId), pid(pid), added_time(added_time), job_status(JobStatus::Running) {
}

JobsList::JobsList() {
	max_jid = 0;
}

void JobsList::addJob(string cmd_line, pid_t pid, int jid) {
	if (jid == 0) {
		JobEntry new_entry(cmd_line, ++max_jid, pid, time(nullptr));
		jobs.push_back(new_entry);
	} else {
		JobEntry new_entry(cmd_line, jid, pid, time(nullptr));
		jobs.push_back(new_entry);
	}
}

void JobsList::printJobsList() {
	// sorting jobs list according to jid
	sort(jobs.begin(), jobs.end());

	vector<JobEntry>::iterator it = jobs.begin();
	while (it!=jobs.end()) {
		time_t elapsed = time(nullptr) - it->getAddedTime();
		cout << "[" << it->getJobId() << "] " << it->getCmdLine() << " : " << it->getJobPid();
		cout << " " << to_string(elapsed) << " secs";
		if (it->getJobStatus() == JobStatus::Stopped) {
			cout << " (stopped)";
		}
		cout << endl;
		it++;
	}
}

void JobsList::killAllJobs() {
    vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
	    pid_t pid = it->getJobPid();
        if (kill(pid, SIGKILL) == -1) {
            perror("smash error: kill failed");
        } else {
        	cout << pid << ": " << it->getCmdLine() << endl;
        }
        ++it;
    }
	max_jid = 0;
}

void JobsList::removeFinishedJobs() {
	pid_t pid;
	int val = 0;
	int status = 0;
	vector<JobEntry>::iterator it = jobs.begin();

	while (it != jobs.end()) {
		pid = it->getJobPid();
		val = waitpid(pid, &status, WNOHANG);
		if (val == -1) {
			perror("smash error: waitpid failed");
			continue;
		} else if (val == pid) {
			max_jid = getLastJob()->getJobId();
			it = jobs.erase(it);
		} else {
			++it;
		}
	}
	if (getJobsListSize() != 0) {
		max_jid = getLastJob()->getJobId();
	} else {
		max_jid = 0;
	}
}

void JobsList::removeJobById(int jobId) {
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (it->getJobId() == jobId) {
			jobs.erase(it);
			return;
		} else {
			++it;
		}
	}
	max_jid = getLastJob()->getJobId();
}

void JobsList::moveToFg(JobEntry* job_entry) {
	pid_t pid = job_entry->getJobPid();
	cout << job_entry->getCmdLine() << " : " << pid << endl;
	if (kill(pid, SIGCONT) == -1) {
		perror("smash error: kill failed");
	} else {
		// setting job_status as Running and making it the fg_job
		fg_job = new JobEntry(job_entry->getCmdLine(), job_entry->getJobId(), job_entry->getJobPid(), job_entry->getAddedTime());
		fg_job->setJobStatus(JobsList::JobStatus::Running);
		removeJobById(job_entry->getJobId());
		int status = 0;
		int val = waitpid(pid, &status, WUNTRACED);
		if (val == -1) {
			perror("smash error: waitpid failed");
		}
	}
}

void JobsList::setFgJob(class JobsList::JobEntry * job_entry) {
	fg_job = job_entry;
}

JobsList::JobEntry* JobsList::getFgJob() {
	return fg_job;
}

JobsList::JobEntry* JobsList::getJobById(int jid) {
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (jid == it->getJobId()) {
			return &(*it);
		} else {
			++it;
		}
	}
	return nullptr;
}

JobsList::JobEntry* JobsList::getJobByPid(pid_t pid) {
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (it->getJobPid() == pid) {
			return &(*it);
		} else {
			++it;
		}
	}
	return nullptr;
}

JobsList::JobEntry* JobsList::getLastJob() {
	JobEntry* last_job = nullptr;
	int last_jid = 0;
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (it->getJobId() > last_jid) {
			last_jid = it->getJobId();
			last_job = &(*it);
		}
		++it;
	}
	return last_job;
}

JobsList::JobEntry* JobsList::getLastStoppedJob() {
	JobEntry* last_stopped_job = nullptr;
	int last_stopped_jid = 0;
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (it->getJobStatus() == JobStatus::Stopped && it->getJobId() > last_stopped_jid) {
			last_stopped_jid = it->getJobId();
			last_stopped_job = &(*it);
		}
		++it;
	}
	return last_stopped_job;
}

int JobsList::getJobsListSize() {
	return jobs.size();
}

bool JobsList::containsStoppedJobs() {
	vector<JobEntry>::iterator it = jobs.begin();
	while (it != jobs.end()) {
		if (it->getJobStatus() == JobStatus::Stopped) {
			return true;
		} else {
			++it;
		}
	}
	return false;
}

void JobsCommand::execute() {
	jobs->removeFinishedJobs();
	jobs->printJobsList();
}

void KillCommand::execute() {
	jobs->removeFinishedJobs();

	if (args_num != 3) {
		_printError("kill: invalid arguments");
		return;
	}

	int sig_num = 0;
	int jid = 0;
	try {
		sig_num = stoi(args[1]);
		jid = stoi(args[2]);
	} catch (exception& e) {
		_printError("kill: invalid arguments");
		return;
	}

	if (!jobs->getJobById(jid)) {
		_printError("kill: job-id " + to_string(jid) + " does not exist");
	} else {
		pid_t job_pid = jobs->getJobById(jid)->getJobPid();
		sig_num = (-1)*(sig_num);
		if (kill(job_pid, sig_num) == -1) {
			perror("smash error: kill failed");
		} else {
			cout << "signal number " << sig_num << " was sent to pid " << job_pid << endl;
		}
	}
}

void ForegroundCommand::execute() {
	jobs->removeFinishedJobs();

	if (args_num > 2) {
		_printError("fg: invalid arguments");
		return;
	} else if (args_num > 1) {
		int jid = 0;
		try {
			jid = stoi(args[1]);
		} catch (exception& e) {
			_printError("fg: invalid arguments");
			return;
		}
		// jid specified
		if (!jobs->getJobById(jid)) {
			_printError("fg: job-id " + to_string(jid) + " does not exist");
		} else {
			jobs->moveToFg(jobs->getJobById(jid));
		}
	} else {
		 // jid not specified
		if (jobs->getJobsListSize() == 0) {
			_printError("fg: jobs list is empty");
		} else {
			// get last added job id
			int last_jid = jobs->getLastJob()->getJobId();
			jobs->moveToFg(jobs->getJobById(last_jid));
		}
	}
}

void BackgroundCommand::execute() {
	jobs->removeFinishedJobs();

	if (args_num > 2) {
		_printError("bg: invalid arguments");
		return;
	} else if (args_num > 1) {
		int jid = 0;
		try {
			jid = stoi(args[1]);
		} catch (exception& e) {
			_printError("bg: invalid arguments");
			return;
		}
		// jid specified
		if (!jobs->getJobById(jid)) {
			_printError("bg: job-id " + to_string(jid) + " does not exist");
		} else {
			if (jobs->getJobById(jid)->getJobStatus() == JobsList::JobStatus::Running) {
				_printError("bg: job-id " + to_string(jid) + " is already running in the background");
			} else {
				JobsList::JobEntry* job = jobs->getJobById(jid);
				pid_t pid = job->getJobPid();
				cout << job->getCmdLine() << " : " << pid << endl;
				if (kill(pid, SIGCONT) == -1) {
					perror("smash error: kill failed");
				} else {
					job->setJobStatus(JobsList::JobStatus::Running);
				}
			}
		}
	} else {
		// jid not specified
		if (!jobs->containsStoppedJobs()) {
			_printError("bg: there is no stopped jobs to resume");
		} else {
			// get last stopped job
			JobsList::JobEntry* last_stopped_job = jobs->getLastStoppedJob();
			pid_t pid = last_stopped_job->getJobPid();
			cout << last_stopped_job->getCmdLine() << " : " << pid << endl;
			if (kill(pid, SIGCONT) == -1) {
				perror("smash error: kill failed");
			} else {
				last_stopped_job->setJobStatus(JobsList::JobStatus::Running);
			}
		}
	}
}

void CopyCommand::execute() {
	if (args_num != 3 || args[1] == nullptr || args[2] == nullptr){
		_printError("cp: invalid arguments");
		return;
	}

	const string input{args[1]};
	remove(args[2]);
	const string output{args[2]};

	// creating a buffer vector with 1024 byte allocated
	vector<char> buf(1024);
	// input file descriptor
	int input_fd = open(input.c_str(), O_RDONLY);
	if (input_fd == -1) {
		perror("smash error: open failed");
		return;
	}
	// output file descriptor
	int output_fd = open(output.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0664);
	if (output_fd == -1) {
		perror("smash error: open failed");
		if (close(input_fd) == -1) {
			perror("smash error: close failed");
		}
		return;
	}

	while (true) {
		ssize_t count = read(input_fd, buf.data(), buf.size());
		if (count == 0) {
			break;
		} else if (count == -1) {
			perror("smash error: read failed");
			if (close(input_fd) == -1 || close(output_fd) == -1) {
				perror("smash error: close failed");
			}
			return;
		}
		ssize_t num_written = 0;
		ssize_t num_to_write = count;
		while(num_written < num_to_write){
			ssize_t write_count = write(output_fd, buf.data() + num_written, count - num_written);
			if (write_count == -1) {
				perror("smash error: write failed");
				if (close(input_fd) == -1 || close(output_fd) == -1) {
					perror("smash error: close failed");
				}
				return;
			} else {
				num_written += write_count;
			}
		}
	}
	if (close(input_fd) == -1 || close(output_fd) == -1) {
		perror("smash error: close failed");
	}
	cout << "smash: " << args[1] << " was copied to " << args[2] << endl;
}

SmallShell::SmallShell() {
	last_wd = "";
    curr_wd = _getCWD();
	cmd_history = new CommandsHistory();
	jobs_list = new JobsList();
}

SmallShell::~SmallShell() {
	delete cmd_history;
	delete jobs_list;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command* SmallShell::createCommand(const char* cmd_line, string cmd_s) {
    if (cmd_s == "pwd") {
        return new GetCurrDirCommand(cmd_line);
    } else if (cmd_s == "cd") {
        return new ChangeDirCommand(cmd_line);
    } else if (cmd_s == "history") {
        return new HistoryCommand(cmd_line, cmd_history);
    } else if (cmd_s == "jobs") {
        return new JobsCommand(cmd_line, jobs_list);
    } else if (cmd_s == "kill") {
        return new KillCommand(cmd_line, jobs_list);
    } else if (cmd_s == "showpid") {
        return new ShowPidCommand(cmd_line);
    } else if (cmd_s == "fg") {
        return new ForegroundCommand(cmd_line, jobs_list);
    } else if (cmd_s == "bg") {
        return new BackgroundCommand(cmd_line, jobs_list);
    } else if (cmd_s == "quit") {
        return new QuitCommand(cmd_line, jobs_list);
    } else if (cmd_s == "cp") {
        return new CopyCommand(cmd_line);
    } else {
        return new ExternalCommand(cmd_line, jobs_list);
    }
}

void SmallShell::executeCommand(const char* cmd_line) {
    getpid();
    char* args[COMMAND_MAX_ARGS];
	_parseCommandLine(cmd_line, args);
	string cmd_s = args[0];

    Command* cmd = createCommand(cmd_line, cmd_s);
	cmd_history->addRecord(cmd_line);
    cmd->execute();
    jobs_list->removeFinishedJobs();
}
