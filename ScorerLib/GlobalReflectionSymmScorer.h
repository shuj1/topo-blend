#pragma once
#include "scorer.h"
class GlobalReflectionSymmScorer :
	public Scorer
{
public:
    GlobalReflectionSymmScorer(Structure::Graph* g, int ith, int logLevel=0):Scorer(g, "GlobalReflectionSymmDetector-", ith, logLevel)
    {
		pointsLevel_= 1;
		init();
    }

    double evaluate();
	double evaluate(Eigen::Vector3d &center, Eigen::Vector3d &normal, double maxGlobalSymmScore=1.0);
    
	int pointsLevel_; // 0 for main control points, 1 for all control points, 2 for all points.
	Eigen::MatrixXd cpts_; // control point of the graph
    Eigen::Vector3d center_;// center of the graph by control points, i.e. of the reflection plane
	Eigen::Vector3d normal_; // normal of the reflection plane
	

    std::vector<Eigen::MatrixXd> nodesCpts_; 
    std::vector<Eigen::Vector3d> nodesCenter_; // center of each node by control points, if the diameter of the part is 0, it is not pushed back in this array.
	std::vector<Structure::Node *> nonDegeneratedNodes_;

private:
	void init();
	//todo 遍历潜在对称面；目前：z冲上，所以就是过中心点的xy面或者yx面。
	Eigen::Vector3d findReflectPlane(const Eigen::Vector3d& center);
	int findNearestPart(Eigen::Vector3d &nc, QString & type, double& dist);
};

