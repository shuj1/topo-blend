#include <QFile>
#include <QElapsedTimer>
#include <QDomElement>

#include "StructureGraph.h"
using namespace Structure;

#include "GraphEmbed.h"
#include "GraphDraw2D.h"

Graph::Graph()
{
	property["embeded2D"] = false;
}

Graph::Graph( QString fileName )
{
	property["embeded2D"] = false;

	loadFromFile( fileName );

	property["name"] = fileName;
}

Structure::Graph::Graph( const Graph & other )
{
	nodes = other.nodes;
	edges = other.edges;
	property = other.property;
	adjacency = other.adjacency;
	misc = other.misc;

	// WARNING: not a deep copy!!
	qDebug() << ">>>  WARNING: not a deep copy!!  <<<";
}

Graph::~Graph()
{
    //qDeleteAll( nodes );
    //nodes.clear();
}

QBox3D Graph::bbox()
{
    QBox3D box;

    foreach(Node * n, nodes){
        box.unite( n->bbox().minimum() );
        box.unite( n->bbox().maximum() );
    }

    return box;
}

Node *Graph::addNode(Node * n)
{
    Node * found = getNode( n->id );
    if(found) return found;

    nodes.push_back(n);
    return n;
}

Link Graph::addEdge(QString n1_id, QString n2_id)
{
	return addEdge(getNode(n1_id), getNode(n2_id));
}

Link Graph::addEdge(Node *n1, Node *n2)
{
    n1 = addNode(n1);
    n2 = addNode(n2);

	Vector3 intersectPoint = nodeIntersection(n1, n2);

	Vec4d c1 = n1->approxCoordinates(intersectPoint);
	Vec4d c2 = n2->approxCoordinates(intersectPoint);

    Link e( n1, n2, c1, c2, "none", linkName(n1, n2) );
    edges.push_back(e);

	// Add to adjacency list
	if(adjacency.cols() != nodes.size()) adjacency = MatrixXd::Zero( nodes.size(), nodes.size() );

	adjacency(nodes.indexOf(n1), nodes.indexOf(n2)) = 1 ;
	adjacency(nodes.indexOf(n2), nodes.indexOf(n1)) = 1 ;

    return e;
}

Link Graph::addEdge(Node *n1, Node *n2, Vec4d coord1, Vec4d coord2, QString linkName)
{
	n1 = addNode(n1);
	n2 = addNode(n2);

	Link e( n1, n2, coord1, coord2, "none", linkName );
	edges.push_back(e);

	// Add to adjacency list
	if(adjacency.cols() != nodes.size()) adjacency = MatrixXd::Zero( nodes.size(), nodes.size() );

	adjacency(nodes.indexOf(n1), nodes.indexOf(n2)) = 1 ;
	adjacency(nodes.indexOf(n2), nodes.indexOf(n1)) = 1 ;

	return e;
}

void Graph::removeEdge( Node * n1, Node * n2 )
{
	int edge_idx = -1;

	for(int i = 0; i < (int)edges.size(); i++)
	{
		Link & e = edges[i];

		if((e.n1 == n1 && e.n2 == n2) || (e.n2 == n1 && e.n1 == n2)){
			edge_idx = i;
			break;
		}
	}

	if(edge_idx < 0) return;

	edges.remove(edge_idx);

	adjacency(nodes.indexOf(n1), nodes.indexOf(n2)) = 0;
	adjacency(nodes.indexOf(n2), nodes.indexOf(n1)) = 0;
}

QString Graph::linkName(Node *n1, Node *n2)
{
    return QString("%1 : %2").arg(n1->id).arg(n2->id);
}

Node *Graph::getNode(QString nodeID)
{
    foreach(Node* n, nodes)
	{
        if(n->id == nodeID) 
			return n;
	}

    return NULL;
}

Link *Graph::getEdge(QString id1, QString id2)
{
	for(int i = 0; i < (int)edges.size(); i++)
	{
		Link & e = edges[i];

		QString nid1 = e.n1->id;
		QString nid2 = e.n2->id;

		if( (nid1 == id1 && nid2 == id2) || (nid1 == id2 && nid2 == id1))
			return &e;
	}
	
	return NULL;
}

void Graph::draw()
{
    foreach(Node * n, nodes)
    {
        n->draw();

		if(n->type() == SHEET)
		{
			Sheet * s = (Sheet*) n;

			glPointSize(3);
			glDisable(GL_LIGHTING);

			glColor3d(0,1,0);
			glBegin(GL_POINTS);
			foreach(Vector3 p, s->surface.debugPoints)	glVector3(p);
			glEnd();

			glBegin(GL_TRIANGLES);
			//foreach(std::vector<Vector3> tri, s->surface.generateSurfaceTris(0.4))
			//{
			//	glColor3d(1,0,0); glVector3(tri[0]);
			//	glColor3d(0,1,0); glVector3(tri[1]);
			//	glColor3d(0,0,1); glVector3(tri[2]);
			//}
			glEnd();
		}
    }

    foreach(Link e, edges)
    {
        e.draw();
    }

	glDisable(GL_LIGHTING);
	glColor3d(1,1,0); glPointSize(8);
	glBegin(GL_POINTS); foreach(Vector3 p, debugPoints) glVector3(p); glEnd();

	glColor3d(0,1,1); glPointSize(14);
	glBegin(GL_POINTS); foreach(Vector3 p, debugPoints2) glVector3(p); glEnd();

	glColor3d(1,0,1); glPointSize(20);
	glBegin(GL_POINTS); foreach(Vector3 p, debugPoints3) glVector3(p); glEnd();
	glEnable(GL_LIGHTING);


	// Materialized
	glBegin(GL_QUADS);
	foreach(QuadFace f, cached_mesh.faces)
	{
		glNormal3(cross(cached_mesh.points[f[1]]-cached_mesh.points[f[0]],cached_mesh.points[f[2]]-cached_mesh.points[f[0]]).normalized());
		glVector3(cached_mesh.points[f[0]]);
		glVector3(cached_mesh.points[f[1]]);
		glVector3(cached_mesh.points[f[2]]);
		glVector3(cached_mesh.points[f[3]]);
	}
	glEnd();


	glDisable(GL_LIGHTING);
	glPointSize(15);
	glColor3d(1,1,0);
	glBegin(GL_POINTS);
	foreach(Vector3 p, cached_mesh.debug) glVector3(p);
	glEnd();
	glEnable(GL_LIGHTING);
}

void Graph::draw2D(int width, int height)
{
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

    // Embed structure graph to 2D screen plane
    if(!property["embeded2D"].toBool())
	{
        //GraphEmbed::embed( this );
		GraphEmbed::circleEmbed( this );

		fontImage = QImage(":/images/font.png");
	}

	// Find 2D bounds
	QBox3D bound2D;
	foreach(Node * n, nodes){
		QVector3D center = n->vis_property[NODE_CENTER].value<QVector3D>();
		bound2D.unite(center);
	}
	Scalar boundRadius = 0.5 * bound2D.size().length();

	QVector3D minBound = bound2D.minimum();
	QVector3D corner = QVector3D(qMin(0.0, minBound.x()), qMin(0.0, minBound.y()), qMin(0.0, minBound.z()));
	QVector3D graph_center = 0.5 * QVector3D(width, height,0);
	QVector3D scaling = QVector3D(width, height, 0) / boundRadius;

	// Compute 2D node centers
	QMap<int, Node*> nmap;
	std::vector<Vector3> node_centers;

	foreach(Node * n, nodes){
		QVector3D center = graph_center + (scaling * (n->vis_property[NODE_CENTER].value<QVector3D>() - corner));
		node_centers.push_back(center);
		nmap[nmap.size()] = n;
	}

	int N = nodes.size();

	// Draw edges:
	glColorQt(QColor(0,0,0));
	foreach(Link e, edges)
	{
		Vector3 c1 = node_centers[nmap.key(e.n1)];
		Vector3 c2 = node_centers[nmap.key(e.n2)];
		drawLine(Vec2i(c1.x(), c1.y()), Vec2i(c2.x(), c2.y()), 1);
	}

	// Draw nodes:
	for(int i = 0; i < N; i++)
	{
		glColorQt( (nmap[i]->type() == SHEET ? QColor(50,0,0) : QColor(0,50,0)) );
		Vector3 c = node_centers[i];

		int w = stringWidthGL(qPrintable(nmap[i]->id)) * 1.1;
		int h = stringHeightGL();

		drawRect(Vec2i(c.x(), c.y()), w, h);
	}

	// Draw titles:
	glColorQt(QColor(255,255,255));
	beginTextDraw(fontImage);
	for(int i = 0; i < N; i++){
		Vector3 c = node_centers[i];

		int w = stringWidthGL(qPrintable(nmap[i]->id)) * 1.1;
		int h = stringHeightGL();

		drawStringGL( c.x() - (0.5 * w), c.y() - (0.5 * h), qPrintable(nmap[i]->id) );
	}
	endTextDraw();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
}

void Graph::saveToFile( QString fileName )
{
	if(nodes.size() < 1) return;

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;

	QTextStream out(&file);
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<document>\n";

	// Save nodes
	foreach(Node * n, nodes)
	{
		out << "<node>\n";

		// Type and ID
		out << QString("\t<id>%1</id>\n").arg(n->id);
		out << QString("\t<type>%1</type>\n\n").arg(n->type());

		// Control count
		out << "\t<controls>\n";
		std::vector<int> controlCount = n->controlCount();
		foreach(int c, controlCount)
			out << QString("\t\t<c>%1</c>\n").arg(c);
		out << "\t</controls>\n\n";

		// Control Points
		foreach(Vec3d p, n->controlPoints())
			out << QString("\t<point>%1 %2 %3</point>\n").arg(p.x()).arg(p.y()).arg(p.z());

		// Weights
		out << "\n\t<weights>";
		foreach(Scalar w, n->controlWeights())
			out << QString::number(w) + " ";
		out << "</weights>\n";

		out << "\n</node>\n\n";
	}

	// Save edges
	foreach(Link e, edges)
	{
		out << "<edge>\n";
		out << QString("\t<id>%1</id>\n").arg(e.id);
		out << QString("\t<type>%1</type>\n\n").arg(e.type);
		out << QString("\t<n>%1</n>\n").arg(e.n1->id);
		out << QString("\t<n>%1</n>\n").arg(e.n2->id);
		out << QString("\t<coord>%1 %2 %3 %4</coord>\n").arg(e.coord[0][0]).arg(e.coord[0][1]).arg(e.coord[0][2]).arg(e.coord[0][3]);
		out << QString("\t<coord>%1 %2 %3 %4</coord>\n").arg(e.coord[1][0]).arg(e.coord[1][1]).arg(e.coord[1][2]).arg(e.coord[1][3]);
		out << "\n</edge>\n\n";
	}

	out << "\n</document>\n";
	file.close();
}

void Graph::loadFromFile( QString fileName )
{
	// Clear data
	nodes.clear();
	edges.clear();

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

	QDomDocument mDocument;
	mDocument.setContent(&file, false);    
	
	int degree = 3;

	// For each node
	QDomNodeList node_list = mDocument.firstChildElement("document").elementsByTagName("node");
	int num_nodes = node_list.count();

	for(int i = 0; i < num_nodes; i++)
	{
		QDomNode node = node_list.at(i);

		QString id = node.firstChildElement("id").text();
		QString node_type = node.firstChildElement("type").text();
		
		// Find number of control elements
		QVector<int> control_count;
		QDomNodeList controls_list = node.firstChildElement("controls").toElement().elementsByTagName("c");
		for(int c = 0; c < (int)controls_list.size(); c++)
			control_count.push_back( controls_list.at(c).toElement().text().toInt() );

		// Load all control points
		std::vector<Vec3d> ctrlPoints;
		QDomNode n = node.firstChildElement("point");
		while (!n.isNull()) {
			if (n.isElement()) {
				QStringList point = n.toElement().text().split(" ");
				ctrlPoints.push_back( Vec3d(point[0].toDouble(),point[1].toDouble(),point[2].toDouble()) );
			}
			n = n.nextSiblingElement("point");
		}

		// Load all control weights
		std::vector<Scalar> ctrlWeights;
		QStringList weights = node.firstChildElement("weights").toElement().text().trimmed().split(" ");
		foreach(QString w, weights) ctrlWeights.push_back(w.toDouble());

		// Add node
		if(node_type == CURVE)
			addNode( new Curve( NURBSCurve(ctrlPoints, ctrlWeights, degree, false, true), id) );
		else if(node_type == SHEET)
		{
			if(control_count.size() < 2) continue;

			std::vector< std::vector<Vec3d> > cp = std::vector< std::vector<Vec3d> > (control_count.first(), std::vector<Vec3d>(control_count.last(), Vector3(0)));
			std::vector< std::vector<Scalar> > cw = std::vector< std::vector<Scalar> > (control_count.first(), std::vector<Scalar>(control_count.last(), 1.0));

			for(int u = 0; u < control_count.first(); u++)
			{
				for(int v = 0; v < control_count.last(); v++)
				{
					int idx = (u * control_count.last()) + v;
					cp[u][v] = ctrlPoints[idx];
					cw[u][v] = ctrlWeights[idx];
				}
			}

			addNode( new Sheet( NURBSRectangle(cp, cw, degree, degree, false, false, true, true), id ) );
		}
	}

	// For each edge
	QDomNodeList edge_list = mDocument.firstChildElement("document").elementsByTagName("edge");
	int num_edges = edge_list.count();

	for(int i = 0; i < num_edges; i++)
	{
		QDomNode edge = edge_list.at(i);

		QString id = edge.firstChildElement("id").text();
		QString edge_type = edge.firstChildElement("type").text();

		QDomNodeList n = edge.toElement().elementsByTagName("n");
		
		QString n1_id = n.at(0).toElement().text();
		QString n2_id = n.at(1).toElement().text();

		QDomNodeList coord = edge.toElement().elementsByTagName("coord");

		QStringList c1 = coord.at(0).toElement().text().split(" ");
		QStringList c2 = coord.at(1).toElement().text().split(" ");

		addEdge(getNode(n1_id), getNode(n2_id), Vec4d(c1[0].toDouble(), c1[1].toDouble(), 0, 0), Vec4d(c2[0].toDouble(), c2[1].toDouble(), 0, 0), id);
	}

	file.close();
}

void Graph::materialize( SurfaceMeshModel * m, Scalar voxel_scaling )
{
	QElapsedTimer timer; timer.start();

	cached_mesh.clear();

	QBox3D box = bbox();
	QVector3D b = bbox().size();
	Scalar avg = (b.x() + b.y() + b.z()) / 3.0;
	Scalar voxel_size = (avg / 70) * voxel_scaling;

	DynamicVoxel vox( voxel_size );

	Vector3 half_voxel = -0.5 * Vector3( voxel_size );

	vox.begin();

	foreach(Node * n, nodes)
	{
		QElapsedTimer nodoe_timer; nodoe_timer.start();

		std::vector< std::vector<Vector3> > parts = n->discretized( voxel_size );
		int c = parts.front().size();

		// Curve segments
		if(c == 2)
		{
			foreach(std::vector<Vector3> segment, parts)
			{
				vox.addCapsule( segment.front() + half_voxel, segment.back() + half_voxel, voxel_size * 2 );
			}
		}

		// Sheet triangles
		if(c == 3)
		{
			QBox3D triBox;
			foreach(std::vector<Vector3> segment, parts)
			{
				foreach(Vector3 p, segment) triBox.unite(p + half_voxel);
			}
			vox.addBox( triBox.minimum(), triBox.maximum() );
		}

		qDebug() << "Node built [" << n->id << "] " << nodoe_timer.elapsed() << " ms";
	}

	vox.end();

	qDebug() << "Voxels built " << timer.elapsed() << " ms";

	vox.buildMesh(m, cached_mesh);

    if(m) cached_mesh.clear();
}

Node *Graph::rootBySize()
{
	int idx = 0;
	Scalar maxArea = 0;

	for(int i = 0; i < nodes.size(); i++){
		Vector3 diagonal = nodes[i]->bbox().size();

		double area = (diagonal[0] == 0 ? 1 : diagonal[0]) * 
					  (diagonal[1] == 0 ? 1 : diagonal[1]) * 
					  (diagonal[2] == 0 ? 1 : diagonal[2]);

		if( area > maxArea){
			idx = i;
			maxArea = area;
		}
	}

    return nodes[idx];
}

Node *Graph::rootByValence()
{
	int maxIdx = 0;
	int maxValence = valence(nodes[maxIdx]);

	for(int i = 0; i < (int)nodes.size(); i++)
	{
		int curVal = valence(nodes[i]);
		if(curVal > maxValence)
		{
			maxValence = curVal;
			maxIdx = i;
		}
	}

    return nodes[maxIdx];
}

SurfaceMeshTypes::Vector3 Graph::nodeIntersection( Node * n1, Node * n2 )
{
	Scalar r = 0.1 * qMin(n1->bbox().size().length(), n2->bbox().size().length());

	std::vector< std::vector<Vector3> > parts1 = n1->discretized(r);
	std::vector< std::vector<Vector3> > parts2 = n2->discretized(r);

	Scalar minDist = DBL_MAX;
	int minI = 0, minJ = 0;

	// Compare parts bounding boxes
	for(int i = 0; i < (int)parts1.size(); i++)
	{
		Vector3 mean1(0);
		Scalar r1 = 0;

		foreach(Vector3 p, parts1[i])	mean1 += p;
		mean1 /= parts1[i].size();
		foreach(Vector3 p, parts1[i])	r1 = qMax((p - mean1).norm(), r1);

		for(int j = 0; j < (int)parts2.size(); j++)
		{
			Vector3 mean2(0);
			Scalar r2 = 0;

			foreach(Vector3 p, parts2[j])	mean2 += p;
			mean2 /= parts2[j].size();
			foreach(Vector3 p, parts2[j])	r2 = qMax((p - mean2).norm(), r2);

			Vector3 diff = mean1 - mean2;
			Scalar dist = diff.norm();

			Scalar sphereDist = dist - r1 - r2;

			if(sphereDist <= 0)
			{
				std::vector<Vector3> p1 = parts1[ i ];
				std::vector<Vector3> p2 = parts2[ j ];
				int g1 = p1.size(), g2 = p2.size();
				Vector3 c1,c2;
				Scalar s,t;

				if(g1 == 2 && g2 == 2) ClosestPointSegments		(p1[0],p1[1],  p2[0],p2[1],       s, t, c1, c2);	// curve -- curve
				if(g1 == 2 && g2 == 3) ClosestSegmentTriangle	(p1[0],p1[1],  p2[0],p2[1],p2[2],       c1, c2);	// curve -- sheet
				if(g1 == 3 && g2 == 2) ClosestSegmentTriangle	(p2[0],p2[1],  p1[0],p1[1],p1[2],       c1, c2);	// sheet -- curve
				if(g1 == 3 && g2 == 3) TriTriIntersect			(p1[0],p1[1],p1[2],  p2[0],p2[1],p2[2], c1, c2);	// sheet -- sheet
				
				Scalar dist = (c1 - c2).norm();

				if(dist < minDist)
				{
					minI = i;
					minJ = j;
					minDist = dist;
				}

				//foreach(Vector3 w, parts1[i]) debugPoints2.push_back(w);
				//foreach(Vector3 w, parts2[j]) debugPoints2.push_back(w);
			}
		}
	}

	// Find the exact intersection point
	std::vector<Vector3> p1 = parts1[ minI ];
	std::vector<Vector3> p2 = parts2[ minJ ];
	int g1 = p1.size(), g2 = p2.size();

	Vector3 c1,c2;
	Scalar s,t;

	if(g1 == 2 && g2 == 2) ClosestPointSegments		(p1[0],p1[1],  p2[0],p2[1],       s, t, c1, c2);	// curve -- curve
	if(g1 == 2 && g2 == 3) ClosestSegmentTriangle	(p1[0],p1[1],  p2[0],p2[1],p2[2],       c1, c2);	// curve -- sheet
	if(g1 == 3 && g2 == 2) ClosestSegmentTriangle	(p2[0],p2[1],  p1[0],p1[1],p1[2],       c1, c2);	// sheet -- curve
	if(g1 == 3 && g2 == 3) TriTriIntersect			(p1[0],p1[1],p1[2],  p2[0],p2[1],p2[2], c1, c2);	// sheet -- sheet

	//debugPoints.push_back(c1);
	//debugPoints.push_back(c2);
	//foreach(Vector3 w, p1) debugPoints2.push_back(w);
	//foreach(Vector3 w, p2) debugPoints3.push_back(w);

	return (c1 + c2) / 2.0;
}

void Graph::printAdjacency()
{
	int n = nodes.size();

	qDebug() << "\n\n== Adjacency Matrix ==";

	for(int i = 0; i < n; i++)
	{
		QString line;

		for(int j = 0; j < n; j++)
			line += QString(" %1 ").arg(adjacency(i,j));

		qDebug() << line;
	}

	qDebug() << "======";
}

int Graph::valence( Node * n )
{
	int idx = nodes.indexOf(n);
	return 0.5 * (adjacency.col(idx).sum() + adjacency.row(idx).sum());
}

Structure::Curve* Graph::getCurve( Link * l )
{
	Structure::Node *n1 = l->n1, *n2 = l->n2;
	return (Structure::Curve *) ((n1->type() == Structure::CURVE) ? n1: n2);
}

QMap<Link*, Vec4d> Structure::Graph::linksCoords( QString nodeID )
{
	QMap<Link*, Vec4d> coords;

	for(int i = 0; i < edges.size(); i++)
	{
		Link * l = &edges[i];
		if(!l->hasNode(nodeID)) continue;

		coords[l] = l->getCoord(nodeID);
	}

	return coords;
}

QVector<Link> Structure::Graph::nodeEdges( QString nodeID )
{
	QVector<Link> nodeLinks;
	
	foreach(Link e, edges) if(e.hasNode(nodeID)) 
		nodeLinks.push_back(e);

	return nodeLinks;
}

QList<Link> Graph::furthermostEdges( QString nodeID )
{
	QMap<double, Link> sortedLinks;
	foreach(Link l, nodeEdges(nodeID))
		sortedLinks[l.getCoord(nodeID).norm()] = l;

	return sortedLinks.values();
}