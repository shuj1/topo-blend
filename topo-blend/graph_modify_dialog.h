#pragma once

#include <QDialog>
#include "StructureGraph.h"

namespace Ui {
class GraphModifyDialog;
}

class GraphModifyDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit GraphModifyDialog(Structure::Graph * graph, QWidget *parent = 0);
    ~GraphModifyDialog();
    
private:
    Ui::GraphModifyDialog *ui;
    Structure::Graph * g;

public slots:
    void link();
    void unlink();
    void remove();
	void removeAll();
	void visualizeSelections();

	void updateLists();

signals:
	void updateView();
};
