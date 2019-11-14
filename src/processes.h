#ifndef PROCESSES_H
#define PROCESSES_H

#include <QTableView>
#include <QAction>
#include <QMutex>
#include "worker.h"

struct ProcessList {
	std::string name;
	std::string type; // compute, graphics, or both
	std::string GPUIndex, pid, computeUse, memoryUse, encoding, decoding, vRAM; // integers
	std::string GPUName;

	ProcessList(const std::string &name, const std::string &type,
				const std::string &gpuIdx, const std::string &pid,
				const std::string &sm, const std::string &mem,
				const std::string &enc, const std::string &dec,
				const std::string& vRAM, const std::string &GPUName);
};

class ProcessesWorker : public Worker {
public:
	std::vector<ProcessList> processes;

	void work() override;
	int processesIndexByPid(const std::string &pid);
};

class ProcessesTableView : public QTableView {
	Q_OBJECT
public:
	ProcessesWorker *worker;

	explicit ProcessesTableView(QWidget *parent = nullptr);
	~ProcessesTableView() override;

	void mousePressEvent(QMouseEvent *event) override;

private:
	std::string selectedPid = "";

	void killProcess();

public slots:
	void onDataUpdated();
};

#endif
