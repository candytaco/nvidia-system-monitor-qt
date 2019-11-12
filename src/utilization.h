#ifndef UTILIZATION_H
#define UTILIZATION_H

#include <QWidget>
#include <QPainter>
#include <vector>
#include <QMutex>

#include "constants.h"
#include "worker.h"

struct Point {
    float x; // [0; 1]
    int y; // [0; 100]
    
    Point(float x, int y);
};

struct UtilizationData {
    int level = 0; // current graph point in percents
    int avgLevel = 0, minLevel = 0, maxLevel = 0;
};

struct MemoryData {
    int total, free, used;
};

class UtilizationWorker : public Worker {
public:
    std::vector<Point> *gpoints; // graph points
    UtilizationData *udata;

    UtilizationWorker();
    
    void work() override;
    virtual void receiveData() = 0;
    void deleteSuperfluousPoints(uint index);
    
    ~UtilizationWorker() override;
protected:
    long lastTime = 0;
};

class GPUUtilizationWorker : public UtilizationWorker {
public:
    void receiveData() override;
};

class MemoryUtilizationWorker : public UtilizationWorker {
public:
    MemoryData *memoryData;

    MemoryUtilizationWorker();
    ~MemoryUtilizationWorker() override;

    void receiveData() override;
};

class UtilizationWidget : public QWidget {
    Q_OBJECT
public:
    std::vector<QRect> statusObjectsAreas;
    UtilizationWorker *worker;

	virtual const char *GetName() const {return "widget ";};

    ~UtilizationWidget() override;

    void paintEvent(QPaintEvent*) override;

public slots:
    void onDataUpdated();
};

class GPUUtilization : public UtilizationWidget {
public:
    GPUUtilization();

	virtual const char *GetName() const override {return "GPU use ";};

    void mouseMoveEvent(QMouseEvent *event) override;
};

class MemoryUtilization : public UtilizationWidget {
public:
    MemoryUtilization();

	virtual const char *GetName() const override {return "Memory use ";};

    void mouseMoveEvent(QMouseEvent *event) override;
};

void drawGrid(QWidget *widget, QPainter *p, const char *name);
void drawGraph(UtilizationWorker *worker, QPainter *p);
void drawStatusObjects(std::vector<QRect> &statusObjectsAreas, UtilizationData *udata, QPainter *p);

#endif
