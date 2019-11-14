#include "utilization.h"
#include <QFontMetrics>
#include <QApplication>
#include <QToolTip>
#include <QMouseEvent>

#include "settings.h"
#include "utils.h"
#include "constants.h"

#define graphHeightCoef 9
int grapthStartY, grapthEndY, width;

void drawGrid(QWidget* widget, QPainter* p, const char* name)
{
	QFontMetrics fm(qApp->font());
	int x0, y0 = fm.height(), graphHeight = graphHeightCoef * y0;
	width = widget->size().width() - 4;

	p->setPen(QColor(100, 100, 100));
	grapthStartY = y0 * 1.25f;
	grapthEndY = grapthStartY + graphHeight;
	for (float i = 0; i <= 1.0f; i += 0.25f)
	{
		p->drawLine(width * i, grapthStartY, width * i, grapthStartY + graphHeight);
		p->drawLine(0, grapthStartY + graphHeight * i, width, grapthStartY + graphHeight * i);
	}

	p->setPen(QApplication::palette().text().color());
	p->drawText(0, y0, name);
	p->drawText(0, grapthEndY + y0, (toString(GRAPH_LENGTH / 1000.0f) + " sec").c_str());

	QString text = "100%";
	x0 = fm.horizontalAdvance(text);
	p->drawText(width - x0, y0, text);

	text = "0%";
	x0 = fm.horizontalAdvance(text);
	p->drawText(width - x0, grapthEndY + y0, text);
}

void drawGraph(UtilizationWorker* worker, QPainter* p)
{
	int x1, y1, x2, y2;
	QColor color;
	QPen pen;
	pen.setWidth(2);
	for (int g = 0; g < GPU_COUNT; g++)
	{
		color = gpuColors[g];
		pen.setColor(color);
		p->setPen(pen);
		for (size_t i = 1; i < worker->graphPoints[g].size(); i++)
		{
			x1 = worker->graphPoints[g][i - 1].x * width;
			y1 = grapthEndY - (grapthEndY - grapthStartY) / 100.0f * worker->graphPoints[g][i - 1].y;
			x2 = worker->graphPoints[g][i].x * width;
			y2 = grapthEndY - (grapthEndY - grapthStartY) / 100.0f * worker->graphPoints[g][i].y;

			QPainterPath filling;
			filling.moveTo(x1, y1);
			filling.lineTo(x2, y2);
			filling.lineTo(x2, grapthEndY);
			filling.lineTo(x1, grapthEndY);
			filling.lineTo(x1, y1);
			color.setAlpha(64);
			p->fillPath(filling, QBrush(color));

			p->drawLine(x1, y1, x2, y2);
		}
	}
}

void drawStatusObjects(std::vector<QRect>& statusObjectsAreas, UtilizationData* utilizationData, QPainter* p)
{
	statusObjectsAreas.clear();
	QFontMetrics fontMetric(qApp->font());
	int size = fontMetric.height() * 2; 					// width and height for progress arc
	int spanAngle, x, y, textWidth, nameWidth;
	int blockSize, horizontalCount;
	QRect progress;

	for (int GPU = 0; GPU < GPU_COUNT; GPU++)
	{

		if (utilizationData[GPU].maximum == 100.0)
			textWidth = fontMetric.horizontalAdvance("100%");	// calc max width
		else
			textWidth = fontMetric.horizontalAdvance("00000 / 00000 MB");
		nameWidth = fontMetric.horizontalAdvance(utilizationData[GPU].name.c_str());
		textWidth = textWidth > nameWidth ? textWidth : nameWidth;
		blockSize = size + STATUS_OBJECT_TEXT_OFFSET + textWidth + STATUS_OBJECT_OFFSET;
		horizontalCount = (width + STATUS_OBJECT_OFFSET) / blockSize; // (width + STATUS_OBJECT_OFFSET) because last element has offset

		p->setPen(gpuColors[GPU]);
		p->setBrush(QBrush(gpuColors[GPU]));

		x = blockSize * (GPU % horizontalCount);
		y = grapthEndY + fontMetric.height() + (size + STATUS_OBJECT_OFFSET) * (GPU / horizontalCount) + GRAPTH_OFFSET;
		spanAngle = -utilizationData[GPU].level / utilizationData[GPU].maximum * 360;

		progress = QRect(x, y, size, size);

		QPainterPath progressPath;
		progressPath.moveTo(x + size / 2, y + size / 2);
		progressPath.arcTo(progress, 90, spanAngle);
		p->drawPath(progressPath);

		p->setPen(QApplication::palette().text().color());
		p->setBrush(QBrush());
		p->drawEllipse(x, y, size, size);

		if (utilizationData[GPU].maximum == 100.0f)
		{
			p->drawText(x + size + STATUS_OBJECT_TEXT_OFFSET, y + size / 2 - fontMetric.xHeight() / 2, (utilizationData[GPU].name.c_str()));
			p->drawText(x + size + STATUS_OBJECT_TEXT_OFFSET, y + size / 2 + int(fontMetric.xHeight() * 1.5), (std::to_string(utilizationData[GPU].level) + "%").c_str());
		}
		else
		{
			p->drawText(x + size + STATUS_OBJECT_TEXT_OFFSET, y + size / 2 - fontMetric.xHeight() / 2, (utilizationData[GPU].name.c_str()));
			p->drawText(x + size + STATUS_OBJECT_TEXT_OFFSET, y + size / 2 + int(fontMetric.xHeight() * 1.5), (std::to_string(utilizationData[GPU].level) + " / " + std::to_string(int(utilizationData[GPU].maximum)) + " MB").c_str());
		}

		statusObjectsAreas.emplace_back(x, y, blockSize, size);
	}
}

Point::Point(const float x, const int y)
{
	this->x = x;
	this->y = y;
}

UtilizationWorker::UtilizationWorker()
{
	graphPoints = new std::vector<Point>[GPU_COUNT];
	utilizationData = new UtilizationData[GPU_COUNT];
}

void UtilizationWorker::work()
{
	mutex.lock();

	if (lastTime == 0)
	{
		lastTime = getTime();
		mutex.unlock();
		return;
	}

	receiveData();

	float step = (float)(getTime() - lastTime) / UPDATE_DELAY * GRAPH_STEP;

	for (int GPU = 0; GPU < GPU_COUNT; GPU++)
	{
		for (Point& i : graphPoints[GPU])
			i.x -= step;

		graphPoints[GPU].emplace_back(1.0f, utilizationData[GPU].level * 100 / utilizationData[GPU].maximum);
		deleteSuperfluousPoints(GPU);

		// calculate average, min, max
		utilizationData[GPU].avgLevel = utilizationData[GPU].maxLevel = 0;
		utilizationData[GPU].minLevel = 100;
		for (size_t i = 0; i < graphPoints[GPU].size(); i++)
		{
			utilizationData[GPU].avgLevel += graphPoints[GPU][i].y;
			if (utilizationData[GPU].maxLevel < graphPoints[GPU][i].y)
				utilizationData[GPU].maxLevel = graphPoints[GPU][i].y;
			if (utilizationData[GPU].minLevel > graphPoints[GPU][i].y)
				utilizationData[GPU].minLevel = graphPoints[GPU][i].y;
		}
		utilizationData[GPU].avgLevel /= graphPoints[GPU].size();
	}

	mutex.unlock();
	dataUpdated();

	lastTime = getTime();
}

void UtilizationWorker::deleteSuperfluousPoints(const uint index)
{
	if (graphPoints[index].size() > 2 && graphPoints[index][0].x < 0 && graphPoints[index][1].x <= 0)
		graphPoints[index].erase(graphPoints[index].begin());
}

UtilizationWorker::~UtilizationWorker()
{
	delete[] graphPoints;
	delete[] utilizationData;
}

void GPUUtilizationWorker::receiveData()
{
	std::vector<std::string> lines = split(exec(NVSMI_CMD_GPU_UTILIZATION), "\n");
	std::vector<std::string> GPUs = split(streamline(exec(NVSMI_LIST_GPUS)), "\n");
	// fake, "emulated" GPUs
//    lines[2] = std::to_string(50 + rand() % 20) + " %";
//    lines.push_back(std::to_string(30 + rand() % 10) + " %");
//    lines.push_back(std::to_string(70 + rand() % 30) + " %");
//    lines.push_back("");
	for (size_t i = 1; i < lines.size() - 1; i++)
	{
		utilizationData[i - 1].name = GPUs[i];
		utilizationData[i - 1].level = std::atoi(split(lines[i], " ")[0].c_str());
	}
}

MemoryUtilizationWorker::MemoryUtilizationWorker() : UtilizationWorker()
{
	memoryData = new MemoryData[GPU_COUNT];
}

MemoryUtilizationWorker::~MemoryUtilizationWorker()
{
	delete[] memoryData;
}

void MemoryUtilizationWorker::receiveData()
{
	std::vector<std::string> lines = split(exec(NVSMI_CMD_MEM_UTILIZATION), "\n"), data;
	std::vector<std::string> GPUs = split(streamline(exec(NVSMI_LIST_GPUS)), "\n");
	for (size_t GPU = 1; GPU < lines.size() - 1; GPU++)
	{
		data = split(lines[GPU], ", ");
		memoryData[GPU - 1].total = std::atoi(split(data[1], " ")[0].c_str());
		memoryData[GPU - 1].free = std::atoi(split(data[2], " ")[0].c_str());
		memoryData[GPU - 1].used = std::atoi(split(data[3], " ")[0].c_str());
		utilizationData[GPU - 1].level = memoryData[GPU - 1].used;
		utilizationData[GPU - 1].maximum = memoryData[GPU - 1].total;
		utilizationData[GPU - 1].name = GPUs[GPU];
	}
}

void UtilizationWidget::paintEvent(QPaintEvent*)
{
	QPainter p;
	p.begin(this);
	p.setRenderHint(QPainter::Antialiasing);
	drawGrid(this, &p, this->GetName());
	QMutexLocker locker(&worker->mutex);
	drawGraph(worker, &p);
	drawStatusObjects(statusObjectsAreas, worker->utilizationData, &p);
	p.end();
}

void UtilizationWidget::onDataUpdated()
{
	update();
}

UtilizationWidget::~UtilizationWidget()
{
	delete worker;
}

GPUUtilization::GPUUtilization()
{
	worker = new GPUUtilizationWorker;
	setMouseTracking(true);
}

#define area statusObjectsAreas[i]

void GPUUtilization::mouseMoveEvent(QMouseEvent* event)
{
	for (size_t i = 0; i < statusObjectsAreas.size(); i++)
	{
		if ((area.x() <= event->x()) && (area.x() + area.width() >= event->x()) && (area.y() <= event->y()) && (area.y() + area.height() >= event->y()))
		{
			QToolTip::showText(event->globalPos(), "GPU Utilization: " + QString::number(worker->utilizationData[i].level) +
												   "\nAverage: " + QString::number(worker->utilizationData[i].avgLevel) +
												   "\nMin: " + QString::number(worker->utilizationData[i].minLevel) +
												   "\nMax: " + QString::number(worker->utilizationData[i].maxLevel));

			return;
		}
	}
}

MemoryUtilization::MemoryUtilization()
{
	worker = new MemoryUtilizationWorker;
	setMouseTracking(true);
}

void MemoryUtilization::mouseMoveEvent(QMouseEvent* event)
{
	for (size_t i = 0; i < statusObjectsAreas.size(); i++)
	{
		if ((area.x() <= event->x()) && (area.x() + area.width() >= event->x()) && (area.y() <= event->y()) && (area.y() + area.height() >= event->y()))
		{
			QToolTip::showText(event->globalPos(),
							   "Memory Utilization: " + QString::number(worker->utilizationData[i].level) +
							   "\nAverage: " + QString::number(worker->utilizationData[i].avgLevel) +
							   "\nMin: " + QString::number(worker->utilizationData[i].minLevel) +
							   "\nMax: " + QString::number(worker->utilizationData[i].maxLevel) +
							   "\nTotal: " + QString::number(((MemoryUtilizationWorker*)worker)->memoryData[i].total) + " MiB" +
							   "\nFree: " + QString::number(((MemoryUtilizationWorker*)worker)->memoryData[i].free) + " MiB" +
							   "\nUsed: " +  QString::number(((MemoryUtilizationWorker*)worker)->memoryData[i].used) + " MiB");

			return;
		}
	}
}
