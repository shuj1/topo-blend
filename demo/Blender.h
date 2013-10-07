#pragma once
#include <QElapsedTimer>
#include "DemoPage.h"
#include "GraphCorresponder.h"
#include "TopoBlender.h"
#include "Scheduler.h"

// Forward Declare
class ProgressItem; class SynthesisManager;
class BlendPathRenderer; class BlendRenderItem; class BlendPathSubButton;

// Blend path container
struct BlendPath{
	Structure::Graph * source;
	Structure::Graph * target;
	GraphCorresponder * gcorr;
	QSharedPointer<Scheduler> scheduler;
	QSharedPointer<TopoBlender> blender;
};

class Blender : public DemoPage
{
    Q_OBJECT
public:
    explicit Blender(Scene * scene, QString title);
    void setupBlendPathItems();

	int numSuggestions, numInBetweens;
	int itemHeight;
	int graphItemWidth;

	int resultsPage;
	QVector<ScheduleType> allSchedules;

	QSharedPointer<Scheduler> m_scheduler;
	QSharedPointer<TopoBlender> m_blender;
	QVector< BlendPath > blendPaths;
	QList< QSharedPointer<Scheduler> > jobs;

	friend class BlendPathRenderer;
	friend class BlendPathSubButton;
	friend class BlendPathSub;

signals:
    void blendPathsReady();
    void blendPathsDone();
	void blendStarted();
	void blendDone();

	void exportShape(QString name, PropertyMap data);

public slots:
    void show();
    void hide();
	void keyReleased(QKeyEvent* keyEvent);
	void cleanUp();
	void clearResults();

	void setGraphCorresponder(GraphCorresponder *);
	void preparePaths();
	void schedulePaths( const QSharedPointer<Scheduler> & scheduler, const QSharedPointer<TopoBlender> & blender );
	void pathProgressChanged();
	void synthDataReady();
	void computeBlendPaths();
	void computeBlendPathsThread();

	void blenderDone();
	void blendResultDone(QGraphicsItem* item);
	void blenderAllResultsDone();

	void showPrevResults();
	void showNextResults();
	void showResultsPage();

	void exportSelected();
	void saveJob();
	QWidget * viewer();

	void previewItem(BlendRenderItem*);

private:
	QVector< QGraphicsItemGroup* > blendPathsItems;
	QVector< QVector< QSharedPointer<BlendRenderItem> > > resultItems;
	QVector< QVector< QSharedPointer<BlendPathSubButton> > > blendSubItems;
	QVector< QGraphicsItem* > auxItems;
	
	QSharedPointer<SynthesisManager> s_manager;

	bool isSample;
	bool isFinished;

    void computePath(const int &index);

	void addBlendSubItem(double x, double y, double w, double h, int i, int j);

	GraphCorresponder * m_gcorr;
	BlendPathRenderer * renderer;
	BlendPathRenderer * resultViewer;
	ProgressItem * progress;

	QElapsedTimer pathsTimer;
	QElapsedTimer synthTimer;
	QElapsedTimer blendTimer;
	QElapsedTimer renderTimer;
};
