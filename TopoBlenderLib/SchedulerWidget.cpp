#include "SchedulerWidget.h"
#include "ui_SchedulerWidget.h"

SchedulerWidget::SchedulerWidget(Scheduler * scheduler, QWidget *parent) : QWidget(parent),
    ui(new Ui::SchedulerWidget), s(scheduler)
{
    ui->setupUi(this);
    ui->timelineView->setScene(scheduler);
	ui->timelineView->updateSceneRect(scheduler->sceneRect());

	// Add nodes to list
	foreach(QGraphicsItem * item, scheduler->items())
	{
		Task * t = (Task*) item;
		ui->nodesList->addItem(t->node->id);
	}
}

SchedulerWidget::~SchedulerWidget()
{
    delete ui;
}