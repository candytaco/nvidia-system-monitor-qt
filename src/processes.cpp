#include "processes.h"
#include <QStandardItem>
#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QMutexLocker>
#include "constants.h"
#include "utils.h"

ProcessList::ProcessList(const std::string& name, const std::string& type,
						 const std::string& gpuIdx, const std::string& pid,
						 const std::string& sm, const std::string& mem,
						 const std::string& enc, const std::string& dec,
						 const std::string& vRAM, const std::string& GPUName)
{
	this->name = name;
	if (type.length() > 1)
		this->type = std::string("Compute + Graphics");
	else
		this->type = type.compare("G") == 0 ? std::string("Graphics") : std::string("Compute");
	this->GPUIndex = gpuIdx;
	this->pid = pid;
	this->computeUse = sm;
	this->memoryUse = mem;
	this->encoding = enc;
	this->decoding = dec;
	this->vRAM = vRAM;
	this->vRAM.append(" MB");
	this->computeUse.append(" %");
	this->GPUName = GPUName;
}

void ProcessesWorker::work() {
	mutex.lock();
	std::vector<std::string> lines = split(streamline(exec(NVSMI_CMD_PROCESSES)), "\n");
	std::vector<std::string> GPUs = split(streamline(exec(NVSMI_LIST_GPUS)), "\n");
	std::vector<std::string> data;
	processes.clear();

	for (size_t line = 2; line < lines.size(); line++) {
		if (lines[line].empty())
			continue;

		data = split(lines[line], " ");

		processes.emplace_back(data[NVSMI_NAME], data[NVSMI_TYPE],
							   data[NVSMI_GPUINDEX], data[NVSMI_PID],
							   data[NVSMI_SM], data[NVSMI_MEM],
							   data[NVSMI_ENC], data[NVSMI_DEC],
							   data[NVSMI_FB], GPUs[std::atoi(data[NVSMI_GPUINDEX].c_str()) + 1]);
	}

	mutex.unlock();

	dataUpdated();
}

int ProcessesWorker::processesIndexByPid(const std::string& pid) {
	for (size_t i = 0; i < processes.size(); i++)
		if (processes[i].pid == pid)
			return i;

	return -1;
}

ProcessesTableView::ProcessesTableView(QWidget *parent) : QTableView(parent) {
	worker = new ProcessesWorker;

	auto *model = new QStandardItemModel;
 
	// Column titles
	QStringList horizontalHeader;
	horizontalHeader.append("Name");
	horizontalHeader.append("Type");
	horizontalHeader.append("GPU");
	horizontalHeader.append("Process ID");
	horizontalHeader.append("Compute Use");
	horizontalHeader.append("GPU Memory Use");
	horizontalHeader.append("Encoding");
	horizontalHeader.append("Decoding");

	model->setHorizontalHeaderLabels(horizontalHeader);

	setModel(model);
	resizeRowsToContents();
	resizeColumnsToContents();
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setEditTriggers(QAbstractItemView::NoEditTriggers);
	verticalHeader()->hide();
	setAutoScroll(false);
}

ProcessesTableView::~ProcessesTableView() {
	delete worker;
}

void ProcessesTableView::mousePressEvent(QMouseEvent *event) {
	QTableView::mousePressEvent(event);
	int row = indexAt(event->pos()).row();

	if (row != -1) {
		QMutexLocker locker(&worker->mutex);
		setCurrentIndex(model()->index(row, 0));
		selectedPid = worker->processes[row].pid;
	} else
		selectedPid = "";

	if (event->button() == Qt::RightButton && row != -1) {
		QMenu contextMenu(tr("Context menu"), this);

		worker->mutex.lock();
		QAction action1(
			("Kill " + worker->processes[row].name + " (pid " + worker->processes[row].pid + ")").c_str(),
			this
		);
		worker->mutex.unlock();
		connect(&action1, &QAction::triggered, this, &ProcessesTableView::killProcess);
		contextMenu.addAction(&action1);

		contextMenu.exec(mapToGlobal(event->pos()));
	}
}

void ProcessesTableView::killProcess() {
	QMutexLocker locker(&worker->mutex);
	if (worker->processesIndexByPid(selectedPid) != -1) {
		exec("kill " + selectedPid);
		selectedPid = "";
	}
}

#define _setItem(row, column, str) \
	((QStandardItemModel*)model())->setItem(row, column, new QStandardItem(QString(worker->processes[i].str.c_str())))

void ProcessesTableView::onDataUpdated() {
	model()->removeRows(0, model()->rowCount());
	QMutexLocker locker(&worker->mutex);

	for (size_t i = 0; i < worker->processes.size(); i++) {
		_setItem(i, NVSM_NAME, name);
		_setItem(i, NVSM_TYPE, type);
		_setItem(i, NVSM_GPUIDX, GPUName);
		_setItem(i, NVSM_PID, pid);
		_setItem(i, NVSM_SM, computeUse);
		_setItem(i, NVSM_MEM, vRAM);
		_setItem(i, NVSM_ENC, encoding);
		_setItem(i, NVSM_DEC, decoding);
	}

	int index = worker->processesIndexByPid(selectedPid);
	if (index != -1)
		setCurrentIndex(model()->index(index, 0));

	resizeColumnsToContents();
}
