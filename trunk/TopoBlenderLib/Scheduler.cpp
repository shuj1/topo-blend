#include <QApplication> // For mouse icon changing
#include <QtConcurrentRun> // For easy multi-threading

#include "TaskCurve.h"
#include "TaskSheet.h"
#include "Scheduler.h"
#include "Relink.h"

#include "Synthesizer.h"
#include <QQueue>

#include "TaskGroups.h"

#include "Scheduler.h"
#include "SchedulerWidget.h"

Q_DECLARE_METATYPE( QSet<int> ) // for tags

Scheduler::Scheduler()
{
	rulerHeight = 25;

	activeGraph = targetGraph = NULL;
	slider = NULL;
	widget = NULL;

	time_step = 0.01;
}

void Scheduler::drawBackground( QPainter * painter, const QRectF & rect )
{
	QGraphicsScene::drawBackground(painter,rect);

	int y = rect.y();
	int screenBottom = y + rect.height();

	// Draw tracks
	for(int i = 0; i < (int)tasks.size() * 1.25; i++)
	{
		int y = i * 17;
		painter->fillRect(-10, y, 4000, 16, QColor(80,80,80));
	}

	// Draw current time indicator
	int ctime = slider->currentTime();
	painter->fillRect(ctime, 0, 1, screenBottom, QColor(0,0,0,128));

	// Draw tags for interesting topology changes
	{
		int tagWidth = 5;
		QSet<int> timeTags = property["timeTags"].value< QSet<int> >();

		foreach(int tagTime, timeTags)
		{
			painter->fillRect(tagTime - (0.5 * tagWidth), 0, tagWidth, screenBottom, QColor(100,100,100,128));
		}
	}
}

void Scheduler::drawForeground( QPainter * painter, const QRectF & rect )
{
	int x = rect.x();
	int y = rect.y();

	// Draw ruler
	int screenBottom = y + rect.height();
	painter->fillRect(x, screenBottom - rulerHeight, rect.width(), rulerHeight, QColor(64,64,64));

	// Draw yellow line
	int yellowLineHeight = 2;
	painter->fillRect(x, screenBottom - rulerHeight - yellowLineHeight, rect.width(), yellowLineHeight, Qt::yellow);

	// Draw text & ticks
	int totalTime = totalExecutionTime();
	int spacing = totalTime / 10;
	int timeEnd = 10;
	int minorTicks = 5;
	painter->setPen(Qt::gray);
	QFontMetrics fm(painter->font());

	for(int i = 0; i <= timeEnd; i++)
	{
		double time = double(i) / timeEnd;

		int curX = i * spacing;

		QString tickText = QString("%1").arg(time);
		painter->drawText(curX - (fm.width(tickText) * 0.5), screenBottom - 14, tickText );

		// Major tick
		painter->drawLine(curX, screenBottom, curX, screenBottom - 10);

		if(i != timeEnd)
		{
			// Minor tick
			for(int j = 1; j < minorTicks; j++)
			{
				double delta = double(spacing) / minorTicks;
				int minorX = curX + (j * delta);
				painter->drawLine(minorX, screenBottom, minorX, screenBottom - 5);
			}
		}
	}

	slider->forceY(screenBottom - rulerHeight - 10);
	slider->setY(slider->myY);
	painter->translate(slider->pos());
	slider->paint(painter, 0, 0);
}

void Scheduler::generateTasks()
{
	foreach(QString snodeID, superNodeCorr.keys())
	{
		QString tnodeID = superNodeCorr[snodeID];

		Task * task;

		if(activeGraph->getNode(snodeID)->type() == Structure::CURVE)
		{
			if (snodeID.contains("null"))  // Grow
				task = new TaskCurve( activeGraph, targetGraph, Task::GROW, tasks.size() );
			else if (tnodeID.contains("null")) // Shrink
				task = new TaskCurve( activeGraph, targetGraph, Task::SHRINK, tasks.size() );
			else
				task = new TaskCurve( activeGraph, targetGraph, Task::MORPH, tasks.size() );
		}

		if(activeGraph->getNode(snodeID)->type() == Structure::SHEET)
		{
			if (snodeID.contains("null"))  // Grow
				task = new TaskSheet( activeGraph, targetGraph, Task::GROW, tasks.size() );
			else if (tnodeID.contains("null")) // Shrink
				task = new TaskSheet( activeGraph, targetGraph, Task::SHRINK, tasks.size() );
			else
				task = new TaskSheet( activeGraph, targetGraph, Task::MORPH, tasks.size() );
		}

		task->setNode( snodeID );
		tasks.push_back( task );
	}
}

void Scheduler::schedule()
{
	Task *current, *prev = NULL;

	foreach(Task * task, tasks)
	{
		// Create
		current = task;

		// Placement
		if(prev) current->moveBy(prev->x() + prev->width,prev->y() + (prev->height));
		current->currentTime = current->x();
		current->start = current->x();

		// Add to scene
		this->addItem( current );

		prev = current;
	}

	// Order and group here:
	this->order();

	// Time-line slider
	if(!slider)
	{
		slider = new TimelineSlider;
		slider->reset();
		this->connect( slider, SIGNAL(timeChanged(int)), SLOT(timeChanged(int)), Qt::QueuedConnection );
		this->addItem( slider );
	}
}

void Scheduler::order()
{
	QMultiMap<Task::TaskType,Task*> tasksByType;

	foreach(Task * task, tasks)
		tasksByType.insert(task->type, task);

	int curStart = 0;
	
	// General layout
	for(int i = Task::SHRINK; i <= Task::GROW; i++)
	{
		QList<Task*> curTasks = tasksByType.values(Task::TaskType(i));

		if(false)
		{
			// Special case: Remove already set tasks during GROW
			{
				QMutableListIterator<Task*> itr(curTasks);
				while (itr.hasNext()) 
					if (itr.next()->property.contains("isTaskAlreadyOrdered")) 
						itr.remove();
			}
		}

		int futureStart = curStart;
		Structure::Graph * g = NULL;

		if(curTasks.size()) 
		{
			// Sort tasks by priority
			curTasks = sortTasksByPriority( curTasks );

			if(i == Task::MORPH)
			{
				foreach(Task* t, curTasks){
					t->setStart(curStart);
					futureStart = qMax(futureStart, t->endTime());
					curStart = futureStart;
				}
			}
			else
			{
				curTasks = sortTasksAsLayers( curTasks, curStart );
				foreach(Task* t, curTasks) futureStart = qMax(futureStart, t->endTime());
			}

			// Group events in same group
			g = (i == Task::SHRINK) ? activeGraph : targetGraph;
			groupStart(g, curTasks, curStart, futureStart);		

			curStart = futureStart;
		}

		if(false)
		{
			// Special case: growing cut null groups
			if(i == Task::SHRINK)
			{
				curTasks = tasksByType.values(Task::GROW);
				if(!curTasks.size()) continue;

				QMutableListIterator<Task*> itr(curTasks);
				while (itr.hasNext()) 
				{
					Structure::Node * n = itr.next()->node();
					if (!n->property.contains("isCutGroup")) itr.remove();
				}
				if(!curTasks.size()) continue;

				curTasks = sortTasksAsLayers( curTasks, curStart );
				foreach(Task* t, curTasks) 
				{
					futureStart = qMax(futureStart, t->endTime());
					t->property["isTaskAlreadyOrdered"] = true;
				}

				g = targetGraph;
				groupStart(g, curTasks, curStart, futureStart);

				curStart = futureStart;

				// Experiment:
				//foreach(Task* t, curTasks) 
				//{
				//	addMorphTask( t->node()->id );
				//}
			}
		}
	}

	// Remove large empty spaces between tasks [inefficient?]
	{
		int curTime = 0;
		forever{
			QList<Task*> before, after;
			splitTasksStartTime(curTime, before, after);

			if(after.empty()) break;

			if(!before.empty())
			{
				int end = endOf( before );
				int start = startOf( after );

				int delta = end - start;
				if(delta < 0) 
					slideTasksTime(after, delta);
			}
			curTime += 50;
		}
	}

	// Add small spaces between tasks
	{
		int timeSpacing = totalExecutionTime() * time_step + 1;

		QVector<Task*> allTasks = tasksSortedByStart();

		int N = allTasks.size();
		for(int i = 0; i < N; i++)
		{
			Task * currTask = allTasks[i];

			QList<Task*> before, after;
			splitTasksStartTime(currTask->endTime() - 1, before, after);
			
			foreach(Task* t, after)
				t->setStart( t->start + timeSpacing );

			while(i < N-1 && allTasks[i+1]->start == currTask->start) i++;
		}
	}

	// To-do: Collect tasks together?
}

void Scheduler::groupStart( Structure::Graph * g, QList<Task*> curTasks, int curStart, int & futureStart )
{
	if(!curTasks.size()) return;

	int i = curTasks.front()->type;

	foreach(QVector<QString> group, g->groups)
	{
		QVector<Task*> tasksInGroup;

		// Check which tasks are in a group
		foreach(Task * t, curTasks){
			Structure::Node * n = (i == Task::SHRINK) ? t->node() : t->targetNode();

			if(group.contains(n->id))
				tasksInGroup.push_back(t);
		}

		curStart = futureStart;

		// Assign same start for them				
		foreach(Task * t, tasksInGroup){
			curStart = qMin(curStart, t->start);		
		}
		foreach(Task * t, tasksInGroup){
			t->setStart(curStart);
			futureStart = qMax(futureStart, t->endTime());
		}
	}
}

QList<Task*> Scheduler::sortTasksByPriority( QList<Task*> currentTasks )
{
	QList<Task*> sorted;

	// Sort by type: Sheets before curves
	QMap<Task*,int> curveTasks, sheetTasks;
	foreach(Task* t, currentTasks)
	{
		if(!t->node()) continue;

		if(t->node()->type() == Structure::CURVE) 
			curveTasks[t] = activeGraph->valence(t->node());
		if(t->node()->type() == Structure::SHEET) 
			sheetTasks[t] = activeGraph->valence(t->node());
	}

	// Sort by valence: Highly connected before individuals
	QList< QPair<int, Task*> > sortedCurve = sortQMapByValue(curveTasks);
	QList< QPair<int, Task*> > sortedSheet = sortQMapByValue(sheetTasks);

	// Combine: Curve[low] --> Curve[high] + Sheet[low] --> Sheet[heigh]
	for (int i = 0; i < (int)sortedCurve.size(); ++i) sorted.push_back( sortedCurve.at(i).second );
	for (int i = 0; i < (int)sortedSheet.size(); ++i) sorted.push_back( sortedSheet.at(i).second );

	// Reverse
	for(int k = 0; k < (sorted.size()/2); k++) sorted.swap(k,sorted.size()-(1+k));

	return sorted;
}

QList<Task*> Scheduler::sortTasksAsLayers( QList<Task*> currentTasks, int startTime )
{
	QList<Task*> sorted;

	QVector< QList<Task*> > groups = TaskGroups::split(currentTasks, activeGraph);

	int futureStart = -1;

	foreach(QList<Task*> group, groups)
	{
		TaskGroups::Graph g( group, activeGraph );

		QVector< QList<Task*> > layers = g.peel();

		if(currentTasks.front()->type == Task::SHRINK)
			layers = reversed(layers);

		foreach(QList<Task*> layer, layers)
		{
			foreach(Task* t, layer){
				t->setStart(startTime);
				futureStart = qMax(futureStart, t->endTime());
			}

			startTime = futureStart;

			sorted += layer;
		}
	}

	return sorted;
}

void Scheduler::reset()
{
	// Save assigned schedule
	QMap< QString,QPair<int,int> > curSchedule = getSchedule();

	allGraphs.clear();
	foreach(Task * task, tasks)	this->removeItem(task);
	tasks.clear();
	property.remove("timeTags");

	// Reload
	this->activeGraph = property["prevActiveGraph"].value<Structure::Graph*>();
	this->targetGraph = property["prevTargetGraph"].value<Structure::Graph*>();

	this->generateTasks();
	this->schedule();

	// Reassign schedule
	this->setSchedule( curSchedule );
	emit( progressChanged(0) );
	emit( hasReset() );
}

void Scheduler::shuffleSchedule()
{
	if(allGraphs.size()) reset();

	QMap< int, QVector<Task*> > startTask;

	foreach(Task * t, this->tasks)
		startTask[t->start].push_back(t);
	
	// These keys are sorted in increasing order
	QVector<int> originalTimes = startTask.keys().toVector();

	// Shuffle them
	QVector<int> startTimes = originalTimes;
	std::random_shuffle(startTimes.begin(), startTimes.end());

	for(int i = 0; i < (int)startTimes.size(); i++)
	{
		int oldStart = originalTimes[i];
		int newStart = startTimes[i];

		QVector<Task*> curTasks = startTask[oldStart];

		foreach(Task * t, curTasks)
		{
			t->setStart( newStart );
		}
	}
}

void Scheduler::executeAll()
{
	double timeStep = time_step;
	int totalTime = totalExecutionTime();
	QVector<Task*> allTasks = tasksSortedByStart();

	// Re-execution
	if( !allGraphs.size() )
	{
		property["prevActiveGraph"].setValue( new Structure::Graph(*activeGraph) );
		property["prevTargetGraph"].setValue( new Structure::Graph(*targetGraph) );
	}

	// pre-execute
	{
		qApp->setOverrideCursor(Qt::WaitCursor);
		property["progressDone"] = false;
		isForceStop = false;

		emit( progressStarted() );
	
		// Tag interesting topology changes
		{
			property.remove("timeTags");

			QSet<int> tags;
			foreach(Task * task, allTasks){
				int time = task->start + (0.5 * task->length);
				tags.insert( time );
			}

			property["timeTags"].setValue( tags );
		}
	}

	Relink linker(this);

	// Initial setup
	{
		// Zero the geometry for null nodes
		foreach(Structure::Node * snode, activeGraph->nodes)
		{
			QString sid = snode->id;
			if (!sid.contains("null")) continue;
			snode->setControlPoints( Array1D_Vector3(snode->numCtrlPnts(), Vector3(0,0,0)) );
			snode->property["zeroGeometry"] = true;

			if( !activeGraph->isCutNode(sid) )
			{
				// Make sure it is "effectively" linked on only one existing node
				QMap<Link *,int> existValence;
				foreach(Link * edge, activeGraph->getEdges(sid)){
					QString otherID = edge->otherNode(sid)->id;
					if(otherID.contains("null")) continue;
					existValence[edge] = activeGraph->valence(activeGraph->getNode(otherID));
				}

				// Ignore if connects to one existing
				if(existValence.size() < 2) 
					continue;

				// Pick existing node with most valence
				QList< QPair<int, Link *> > sorted = sortQMapByValue(existValence);
				Link * linkKeep = sorted.front().second;

				// Replace all edges of null into the kept edge
				foreach(Link * edge, activeGraph->getEdges(sid))
				{
					// Keep track of original edge info
					edge->pushState();

					if(edge == linkKeep) continue;

					QString oldNode = edge->otherNode(sid)->id;
					edge->replace(oldNode, linkKeep->otherNode(sid), linkKeep->getCoordOther(sid));
				}

				snode->property["edgesModified"] = true;
			}
		}

		// Relink once to place null nodes at initial positions:
		QVector<QString> aTs; 
		foreach (Task* task, allTasks) {if ( task->type != Task::GROW)	aTs << task->nodeID;}
		if (aTs.isEmpty()) aTs << allTasks.front()->nodeID;
		activeGraph->property["activeTasks"].setValue( aTs );

		linker.execute();

		// Debug: 
		//allGraphs.push_back( new Structure::Graph( *activeGraph ) );
	}

	// Execute all tasks
	for(double globalTime = 0; globalTime <= (1.0 + timeStep); globalTime += timeStep)
	{
		QElapsedTimer timer; timer.start();

		// DEBUG - per pass
		activeGraph->clearDebug();

		// active tasks
		QVector<QString> aTs = activeTasks(globalTime * totalTime);
		activeGraph->property["activeTasks"].setValue( aTs );

		// For visualization
		activeGraph->setPropertyAll("isActive", false);

		// Blend deltas
		blendDeltas( globalTime, timeStep );

		/// Prepare and execute current tasks
		for(int i = 0; i < (int)allTasks.size(); i++)
		{
			Task * task = allTasks[i];
			double localTime = task->localT( globalTime * totalTime );
			if( localTime < 0 || task->isDone ) continue;

			// Prepare task for grow, shrink, morph
			task->prepare();

			// Execute current task at current time
			task->execute( localTime );

			// For visualization
			if(localTime >= 0.0 && localTime < 1.0) task->node()->property["isActive"] = true;
		}

		/// Geometry morphing
		foreach(Task * task, allTasks)
		{
			double localTime = task->localT( globalTime * totalTime );
			if( localTime < 0 ) continue;
			task->geometryMorph( localTime );
		}

		/// Apply relinking
		linker.execute();

		// Output current active graph:
		allGraphs.push_back(  new Structure::Graph( *activeGraph )  );

		// DEBUG:
		activeGraph->clearDebug();

		// UI - progress visual indicator:
		int percent = globalTime * 100;
		emit( progressChanged(percent) );

		if( isForceStop ) break;
	}

	slider->enable();

	emit( progressDone() );

	qApp->restoreOverrideCursor();
	property["progressDone"] = true;
}

bool Scheduler::isPartOfGrowingBranch( Task* t )
{
	return (t->type == Task::GROW) && !(t->node()->property.contains("isCutGroup"));
}

QVector<Task*> Scheduler::getEntireBranch( Task * t )
{
	QVector<Task*> branch;
	foreach(Structure::Node * n, activeGraph->nodesWithProperty("nullSet", t->node()->property["nullSet"]))
		branch.push_back( getTaskFromNodeID(n->id) );
	return branch;
}

void Scheduler::drawDebug()
{
	foreach(Task * t, tasks)
		t->drawDebug();
}

int Scheduler::totalExecutionTime()
{
	int endTime = 0;

	foreach(Task * t, tasks)
		endTime = qMax(endTime, t->endTime());

	return endTime;
}

void Scheduler::timeChanged( int newTime )
{
	if(!allGraphs.size()) return;

	int idx = allGraphs.size() * (double(newTime) / totalExecutionTime());

	idx = qRanged(0, idx, allGraphs.size() - 1);
	allGraphs[idx]->property["index"] = idx;

	emit( activeGraphChanged(allGraphs[idx]) );
}

void Scheduler::doBlend()
{
	foreach(Task * t, tasks) t->setSelected(false);

	// If we are re-executing, we need to reset everything
	if(allGraphs.size())
		reset();

	/// Execute the tasks on a new thread
	QtConcurrent::run( this, &Scheduler::executeAll ); // scheduler->executeAll();
}

QVector<Task*> Scheduler::tasksSortedByStart()
{
	QMap<Task*,int> tasksMap;
	typedef QPair<int, Task*> IntTaskPair;
	foreach(Task* t, tasks) tasksMap[t] = t->start;
	QList< IntTaskPair > sortedTasksList = sortQMapByValue<Task*,int>( tasksMap );
	QVector< Task* > sortedTasks; 
	foreach( IntTaskPair p, sortedTasksList ) sortedTasks.push_back(p.second);
	return sortedTasks;
}

void Scheduler::stopExecution()
{
	isForceStop = true;
}

void Scheduler::startAllSameTime()
{
	if(selectedItems().isEmpty())
	{
		foreach(Task * t, tasks)
			t->setX(0);
	}
	else
	{
		// Get minimum start
		int minStart = INT_MAX;
		foreach(Task * t, tasks){
			if(t->isSelected())
				minStart = qMin( minStart, t->start );
		}

		foreach(Task * t, tasks){
			if(t->isSelected())
				t->setX( minStart );
		}
	}
}

void Scheduler::startDiffTime()
{
	if(selectedItems().isEmpty())
	{
		for(int i = 0; i < (int)tasks.size(); i++){
			int startTime = 0;
			if(i > 0) startTime = tasks[i - 1]->endTime();
			tasks[i]->setX( startTime );
		}
	}
	else
	{
		int startTime = -1;
		foreach(Task * t, tasks){
			if(t->isSelected())
			{
				if(startTime < 0) startTime = t->start;
				t->setX( startTime );
				startTime += t->length;
			}
		}
	}
}

void Scheduler::prepareSynthesis()
{

}

Task * Scheduler::getTaskFromNodeID( QString nodeID )
{
	foreach(Task * t, tasks) if(t->node()->id == nodeID) return t;
	return NULL;
}

QVector<QString> Scheduler::activeTasks( double globalTime )
{
	QVector<QString> aTs;

	for(int i = 0; i < (int)tasks.size(); i++)
	{
		Task * task = tasks[i];
		double localTime = task->localT( globalTime );

		bool isActive = task->isActive( localTime );

		// Consider future growing cut nodes as active
		bool isUngrownCut = (!task->isDone) && (task->type == Task::GROW) && (task->node()->property.contains("isCutGroup"));

		// HH - Why?
		// Consider dead links as active tasks
		//bool isDeadLink = task->isDone && (task->type == Task::SHRINK);
		bool isDeadLink = false;

		if ( isActive || isUngrownCut || isDeadLink )
		{
			aTs.push_back( task->node()->id );
		}
	}

	return aTs;
}

void Scheduler::splitTasksStartTime( int startTime, QList<Task*> & before, QList<Task*> & after )
{
	foreach(Task * t, tasks){
		if(t->start < startTime)
			before.push_back(t);
		else
			after.push_back(t);
	}
}

void Scheduler::slideTasksTime( QList<Task*> list_tasks, int delta )
{
	foreach(Task * t, list_tasks){
		t->setStart( t->start + delta );
	}
}

int Scheduler::startOf( QList<Task*> list_tasks )
{
	int start = INT_MAX;
	foreach(Task * t, list_tasks) start = qMin(start, t->start);
	return start;
}

int Scheduler::endOf( QList<Task*> list_tasks )
{
	int end = -INT_MAX;
	foreach(Task * t, list_tasks) end = qMax(end, t->endTime());
	return end;
}

void Scheduler::addMorphTask( QString nodeID )
{
	Task * prev = tasks.back();

	Task * task;

	if(activeGraph->getNode(nodeID)->type() == Structure::CURVE)
		task = new TaskCurve( activeGraph, targetGraph, Task::MORPH, tasks.size() );

	if(activeGraph->getNode(nodeID)->type() == Structure::SHEET)
		task = new TaskSheet( activeGraph, targetGraph, Task::MORPH, tasks.size() );

	task->setNode( nodeID );
	tasks.push_back( task );

	// Placement
	task->moveBy(prev->x() + prev->width,prev->y() + (prev->height));
	task->currentTime = task->x();
	task->start = task->x();

	// Add to scene
	this->addItem( task );
}

void Scheduler::blendDeltas( double globalTime, double timeStep )
{
	if (globalTime >= 1.0) return;

	Q_UNUSED( timeStep );
	//double alpha = timeStep / (1 - globalTime);

	foreach(Structure::Link* l, activeGraph->edges)
	{
		Structure::Link* tl = targetGraph->getEdge(l->property["correspond"].toInt());
		if (!tl) continue;

		//Alpha value:
		double alpha = 0;
		{
			Task * sTask1 = l->n1->property["task"].value<Task*>();
			Task * sTask2 = l->n2->property["task"].value<Task*>();

			if(sTask1->isDone && sTask2->isDone) 
			{
				alpha = 1.0;
			}
			else if( sTask1->isDone )
			{
				alpha = sTask2->property["t"].toDouble();
			}
			else
			{
				alpha = sTask1->property["t"].toDouble();
			}
		}

		Vector3d sDelta = l->delta();
		Vector3d tDelta = tl->property["delta"].value<Vector3d>();

		// flip tDelta if is not consistent with sDeltas
		Node *sn1 = l->n1;

		Vector3d blendedDelta = AlphaBlend(alpha, sDelta, tDelta);
		l->property["blendedDelta"].setValue( blendedDelta );

		// Visualization
		activeGraph->vs3.addVector(l->position(sn1->id), blendedDelta);
		//activeGraph->vs.addVector(l->position(sn1->id), tDelta);
	}
}

void Scheduler::setGDResolution( double r)
{
	DIST_RESOLUTION = r;
}

void Scheduler::setTimeStep( double dt )
{
	time_step = dt;
}

void Scheduler::loadSchedule(QString filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
	QFileInfo fileInfo(file.fileName());

	QTextStream in(&file);

	int N = 0; 
	QString nodeID;
	int start = 0, length = 0;

	in >> N; if(N != tasks.size()) { qDebug() << "Invalid schedule!"; return; }

	for(int i = 0; i < N; i++)
	{
		in >> nodeID >> start >> length;
		Task * t = getTaskFromNodeID( nodeID );

		if( t )
		{
			t->setStart( start );
			t->setLength( length );
		}
	}

	file.close();
}

void Scheduler::saveSchedule(QString filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
	QFileInfo fileInfo(file.fileName());

	QTextStream out(&file);
	out << tasks.size() << "\n";
	foreach(Task * t, tasks)	out << t->nodeID << " " << t->start << " " << t->length << "\n";
	file.close();
}

QMap< QString, QPair<int,int> > Scheduler::getSchedule()
{
	QMap< QString, QPair<int,int> > result;
	foreach(Task * t, tasks) result[t->nodeID] = qMakePair( t->start, t->length );
	return result;
}

void Scheduler::setSchedule( QMap< QString, QPair<int,int> > fromSchedule )
{
	Task * t = NULL;
	foreach(QString nodeID, fromSchedule.keys())
	{
		if(t = getTaskFromNodeID(nodeID))
		{
			t->setStart(fromSchedule[nodeID].first);
			t->setLength(fromSchedule[nodeID].second);
		}
	}
}

void Scheduler::defaultSchedule()
{
	this->startDiffTime();
	this->order();
}

void Scheduler::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	emit( updateExternalViewer() );
	QGraphicsScene::mouseReleaseEvent(event);
}

void Scheduler::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
	property["mouseFirstPress"] = true;

	QGraphicsScene::mousePressEvent(event);

	// Reset on changes to schedule
	if(allGraphs.size()){
		foreach(Task* t, tasks){
			if(t->isSelected()){
				this->reset();
				emitUpdateExternalViewer();
			}
		}
	}
}

void Scheduler::mouseMoveEvent( QGraphicsSceneMouseEvent * event )
{
	if(property["mouseFirstPress"].toBool()){
		property["mouseFirstPress"] = false;
		emitUpdateExternalViewer();
	}

	QGraphicsScene::mouseMoveEvent(event);
}

void Scheduler::emitUpdateExternalViewer()
{
	emit( updateExternalViewer() );
}

void Scheduler::emitProgressStarted()
{
	emit( progressStarted() );
}

void Scheduler::emitProgressChanged( int val )
{
	emit( progressChanged(val) );
}

void Scheduler::emitProgressedDone()
{
	emit( progressDone() );
}
