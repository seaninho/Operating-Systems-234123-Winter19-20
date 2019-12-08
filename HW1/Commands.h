#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>

using namespace std;

#define COMMAND_ARGS_MAX_LENGTH (80)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#define BASH_PATH "/bin/bash"

class Command {
protected:
    const char* cmd;
	char* args[COMMAND_MAX_ARGS];
	int args_num;
public:
    explicit Command(const char* cmd_line);
    virtual ~Command() = default;
    virtual void execute() = 0;
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char* cmd_line) : Command(cmd_line) {}
    ~BuiltInCommand() override = default;
};

class JobsList;
class ExternalCommand : public Command {
	JobsList* jobs;
public:
    explicit ExternalCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line), jobs(jobs) {}
    ~ExternalCommand() override = default;
    void execute() override;
};

//class PipeCommand : public Command {
//    // TODO: Add your data members
//public:
//    PipeCommand(const char* cmd_line);
//    virtual ~PipeCommand() {}
//    void execute() override;
//};
//
//class RedirectionCommand : public Command {
//    // TODO: Add your data members
//public:
//    explicit RedirectionCommand(const char* cmd_line);
//    virtual ~RedirectionCommand() {}
//    void execute() override;
//    //void prepare() override;
//    //void cleanup() override;
//};

class ChangeDirCommand : public BuiltInCommand {
public:
    explicit ChangeDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    ~ChangeDirCommand() override = default;
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    ~GetCurrDirCommand() override = default;
    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
    string pid;
public:
    explicit ShowPidCommand(const char* cmd_line);
    ~ShowPidCommand() override = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
	JobsList* jobs;
public:
    QuitCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    ~QuitCommand() override = default;
    void execute() override;
};

class CommandsHistory {
protected:
    class CommandHistoryEntry {
        string cmd_line;
    	int entry_seq_num;
    public:
    	CommandHistoryEntry(const char* cmd_line, int seq_num) : cmd_line(cmd_line), entry_seq_num(seq_num) {}
	    bool operator< (const CommandHistoryEntry& cmd_entry) const {
		    return (entry_seq_num < cmd_entry.entry_seq_num);
	    }
        int getEntrySeqNum() {
            return entry_seq_num;
        }
        string getCmdLine() {
            return cmd_line;
        }
	    void setSeqNum(int seq_num) {
		    this->entry_seq_num = seq_num;
	    }
    };
private:
    int last_entry_idx;
    int history_seq_num;
    vector<CommandHistoryEntry> history;
public:
    CommandsHistory();
    ~CommandsHistory() = default;
    void addRecord(const char* cmd_line);
    void printHistory();
};

class HistoryCommand : public BuiltInCommand {
	CommandsHistory* history;
public:
    HistoryCommand(const char* cmd_line, CommandsHistory* history);
    ~HistoryCommand() override = default;
    void execute() override;
};

class JobsList {
public:
	enum class JobStatus {Running, Stopped};
    class JobEntry {
	    string cmd_line;
	    int jid;
	    pid_t pid;
	    time_t added_time;
	    JobStatus job_status;
    public:
        JobEntry(string cmd_line, int jobId, pid_t pid, time_t added_time);
        ~JobEntry() = default;
	    bool operator< (const JobEntry& job_entry) const {
		    return (jid < job_entry.jid);
	    }
	    string getCmdLine() {
		    return cmd_line;
	    }
        int getJobId() {
        	return jid;
        }
        pid_t getJobPid() {
        	return pid;
        }
        time_t getAddedTime() {
        	return added_time;
        }
        JobStatus getJobStatus() {
	    	return job_status;
	    }
	    void setJobStatus(JobStatus new_status) {
		    job_status = new_status;
	    }
    };
private:
	int max_jid;
	JobEntry* fg_job;
    vector<JobEntry> jobs;
public:
    JobsList();
    ~JobsList() = default;
    void addJob(string cmd_line, pid_t pid, int jid = 0);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    void removeJobById(int jobId);
	void moveToFg(JobEntry* job_entry);
	void setFgJob(JobEntry* job_entry);
    JobEntry* getFgJob();
	JobEntry* getJobById(int jid);
	JobEntry* getJobByPid(pid_t pid);
	JobEntry* getLastJob();
	JobEntry* getLastStoppedJob();
    int getJobsListSize();
    bool containsStoppedJobs();
};

class JobsCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    ~JobsCommand() = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
    JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    ~KillCommand() override = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
   JobsList* jobs;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    ~ForegroundCommand() override = default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
	JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line), jobs(jobs) {}
    ~BackgroundCommand() override = default;
    void execute() override;
};

class CopyCommand : public BuiltInCommand {
public:
    explicit CopyCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {}
    ~CopyCommand() override = default;
    void execute() override;
};


class SmallShell {
private:
    string curr_wd;
    string last_wd;
	CommandsHistory* cmd_history;
    JobsList* jobs_list;
    SmallShell();
public:
    Command* createCommand(const char* cmd_line, string cmd_s);
    SmallShell(SmallShell const &) = delete; // disable copy ctor
    void operator=(SmallShell const &) = delete; // disable = operator
    static SmallShell &getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }
    ~SmallShell();
    void executeCommand(const char* cmd_line);

    string getCurrWd() {
        return curr_wd;
    }
    string getLastWd() {
        return last_wd;
    }
    void setCurrWd(string cwd) {
	    curr_wd = cwd;
    }
    void setLastWd(string lwd) {
	    last_wd = lwd;
    }
    JobsList* getJobsList() {
	    return jobs_list;
    }
};

#endif //SMASH_COMMAND_H_
