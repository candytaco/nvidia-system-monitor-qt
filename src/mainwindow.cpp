#include "mainwindow.h"
#include <QPushButton>
#include <iostream>
#include <QMenuBar>
#include <QApplication>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>

#include "processes.h"
#include "utilization.h"

MainWindow::MainWindow(QWidget*) { 
    auto *layout = new QVBoxLayout;
    layout->setSpacing(0);
    layout->setMargin(0);
    
    auto *menuBar = new QMenuBar;
    auto *menu = new QMenu("&Help");

    menu->addAction("&About NVSM", this, SLOT(about()), Qt::CTRL + Qt::Key_A);
    menu->addAction("&Help", this, SLOT(help()), Qt::CTRL + Qt::Key_H);
    menu->addSeparator();
    menu->addAction("&Settings", this, SLOT(help()));
    menu->addSeparator();
    menu->addAction("&Exit", qApp, SLOT(quit()));

    menuBar->addMenu(menu);
    layout->addWidget(menuBar);
    
    auto *processes = new ProcessesTableView;
    
    auto *gwidget = new QWidget();
    auto *glayout = new QVBoxLayout;
    auto *gutilization = new GPUUtilization;
    glayout->addWidget(gutilization);
    glayout->setMargin(32);
    gwidget->setLayout(glayout);
    auto *mutilization = new MemoryUtilization;
    glayout->addWidget(mutilization);
    
    tabs = new QTabWidget();
    tabs->addTab(processes, "Processes");
    tabs->addTab(gwidget, "GPU Utilization");
    layout->addWidget(tabs);
    
    auto *window = new QWidget();
    window->setLayout(layout);
    setCentralWidget(window);
    
    connect(processes->worker, &ProcessesWorker::dataUpdated, processes, &ProcessesTableView::onDataUpdated);
    connect(gutilization->worker, &GPUUtilizationWorker::dataUpdated, gutilization, &GPUUtilization::onDataUpdated);
    connect(mutilization->worker, &MemoryUtilizationWorker::dataUpdated, mutilization, &MemoryUtilization::onDataUpdated);
    
    workerThread = new WorkerThread;
    workerThread->workers[0] = processes->worker;
    workerThread->workers[1] = gutilization->worker;
    workerThread->workers[2] = mutilization->worker;
    workerThread->start();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    hide();
    workerThread->running = false;
    while (workerThread->isRunning()); // waiting for all workers to be safely removed
    event->accept();
}

void MainWindow::about() {
    QMessageBox::information(
        nullptr,
        "About",
        R"(<font size=4><b>NVIDIA System Monitor</b></font>
        <br>Version 1.1<br>The app monitors your Nvidia GPU<br><br>Developed by Daniel Bernar
        <br><a href='dbcongard@gmail.com'>dbcongard@gmail.com</a>
        <br><br><a href='https://github.com/congard/nvidia-system-monitor-qt/blob/master/DONATE.md'>Donate</a> <a href='https://github.com/congard/nvidia-system-monitor-qt'>GitHub</a> <a href='https://t.me/congard'>Telegram</a>)"
    );
}

void MainWindow::help() {
    QMessageBox msgBox;
    msgBox.setText("<font size=4><b>Help</b></font>");
    msgBox.setInformativeText(
        R"(<b>Settings</b><br>By default, update delay is 2 seconds (2000 ms).
        You most likely want to change this value to, for example, 500 ms. To do this, create file <i>config</i> in the folder
        <i>~/.config/nvidia-system-monitor</i>
        <br><br><b>config values</b>
        <ul>
            <li>updateDelay &lt;time in ms&gt;</li>
            <li>graphLength &lt;time in ms&gt;</li>
            <li>gpuColor &lt;gpu index&gt; &lt;red&gt; &lt;green&gt; &lt;blue&gt;</li>
        </ul><br>
        <b>Processes</b>
        <ul>
            <li>Name - process name</li>
            <li>Type - "C" for Compute Process, "G" for Graphics Process, and "C+G" for the process having both Compute and Graphics contexts</li>
            <li>GPU ID - id of GPU in which process running</li>
            <li>pid - process id</li>
            <li>sm [%]</li>
            <li>mem [%]</li>
            <li>enc [%]</li>
            <li>dec [%]</li>
        </ul><br>
        <b>GPU Utilization</b><br>This section displays a graph of gpu utilization.
        <br><br><b>Memory Utilization</b><br>This section displays a graph of memory utilization.
        <br><br><a href='https://github.com/congard/nvidia-system-monitor-qt/blob/master/DONATE.md'>Donate</a> <a href='https://github.com/congard/nvidia-system-monitor-qt'>GitHub</a> <a href='https://t.me/congard'>Telegram</a>)"
    );
    msgBox.exec();
}
