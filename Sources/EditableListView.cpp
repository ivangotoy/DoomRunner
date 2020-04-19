//======================================================================================================================
// Project: DoomRunner
//----------------------------------------------------------------------------------------------------------------------
// Author:      Jan Broz (Youda008)
// Created on:  2.7.2019
// Description: list view that supports editing of item names and behaves correctly on both internal and external
//              drag&drop operations
//======================================================================================================================

#include "EditableListView.hpp"

#include "ListModel.hpp"
#include "WidgetUtils.hpp"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>


//======================================================================================================================
/* When attempting to make a drag&drop from a new source work properly, there are 3 things to remember:
 *  1. View must support the drop action type the source emits. Some use MoveAction, some CopyAction, ...
 *  2. Model::mimeTypes() must return the MIME type, that is used by the source widget.
 *  3. Model::canDropMimeData(...) must be correctly implemented to support both the MIME type and the drop action
 */

//======================================================================================================================
// idiotic workaround because Qt is fucking retarded
//
// When an internal drag&drop for item reordering is performed, Qt doesn't update the selection and leaves selected
// those items sitting at the old indexes where the drag&drop started and where are now some completely different items.
//
// We can't manually update the indexes in dropEvent, because after dropEvent Qt calls model.removeRows on items
// that are CURRENTLY SELECTED, instead of on items that were selected at the beginning of the drag&drop operation.
// So we must update the selection at some point AFTER the drag&drop operation is finished and the rows removed.
//
// The correct place seems to be (despite its confusing name) QAbstractItemView::startDrag. It is a common
// parent function for Model::dropMimeData and Model::removeRows both of which happen when items are dropped.
// However this is called only when the source of the drag is this application.
// When you drag files from a directory window, then dropEvent is called from somewhere else. In that case
// we update the selection in dropEvent, because there the deletion of the selected items doesn't happen.


//======================================================================================================================

EditableListView::EditableListView( QWidget * parent ) : QListView ( parent )
{
	allowIntraWidgetDnD = true;
	allowInterWidgetDnD = false;
	allowExternFileDnD = false;
	updateDragDropMode();
	setDefaultDropAction( Qt::MoveAction );
	setDropIndicatorShown( true );

	allowEditNames = false;
	setEditTriggers( QAbstractItemView::NoEditTriggers );
}

void EditableListView::updateDragDropMode()
{
	const bool externalDrops = allowInterWidgetDnD || allowExternFileDnD;

	if (!allowIntraWidgetDnD && !externalDrops)
		setDragDropMode( NoDragDrop );
	else if (allowIntraWidgetDnD && !externalDrops)
		setDragDropMode( InternalMove );
	else
		setDragDropMode( DragDrop );
}

void EditableListView::toggleIntraWidgetDragAndDrop( bool enabled )
{
	allowIntraWidgetDnD = enabled;

	updateDragDropMode();
}

void EditableListView::toggleInterWidgetDragAndDrop( bool enabled )
{
	allowInterWidgetDnD = enabled;

	updateDragDropMode();
}

void EditableListView::toggleExternalFileDragAndDrop( bool enabled )
{
	allowExternFileDnD = enabled;

	updateDragDropMode();
}

bool EditableListView::isDropAcceptable( QDragMoveEvent * event )
{
	if (isIntraWidgetDnD( event ) && allowIntraWidgetDnD && event->possibleActions() & Qt::MoveAction)
		return true;
	else if (isInterWidgetDnD( event ) && allowInterWidgetDnD && event->possibleActions() & Qt::MoveAction)
		return true;
	else if (isExternFileDnD( event ) && allowExternFileDnD)
		return true;
	else
		return false;
}

bool EditableListView::isIntraWidgetDnD( QDropEvent * event )
{
	return event->source() == this;
}

bool EditableListView::isInterWidgetDnD( QDropEvent * event )
{
	return event->source() != this && !event->mimeData()->hasUrls();
}

bool EditableListView::isExternFileDnD( QDropEvent * event )
{
	return event->source() != this && event->mimeData()->hasUrls();
}

void EditableListView::dragEnterEvent( QDragEnterEvent * event )
{
	// QListView::dragEnterEvent in short:
	// 1. if mode is InternalMove then discard events from external sources and copy actions
	// 2. accept if event contains at leats one mime type present in model->mimeTypes or model->canDropMimeData
	// We override it, so that we apply our own rules and restrictions for the drag&drop operation.

	if (isDropAcceptable( event )) {  // does proposed drop operation comply with our settings?
		superClass::dragEnterEvent( event );  // let it calc the index and query the model if the drop is ok there
	} else {
		event->ignore();
	}
}

void EditableListView::dragMoveEvent( QDragMoveEvent * event )
{
	// QListView::dragMoveEvent in short:
	// 1. if mode is InternalMove then discard events from external sources and copy actions
	// 2. accept if event contains at leats one mime type present in model->mimeTypes or model->canDropMimeData
	// 3. draw drop indicator according to position
	// We override it, so that we apply our own rules and restrictions for the drag&drop operation.

	if (isDropAcceptable( event )) {  // does proposed drop operation comply with our settings?
		superClass::dragMoveEvent( event );  // let it query the model if the drop is ok there and draw the indicator
	} else {
		event->ignore();
	}
}

void EditableListView::dropEvent( QDropEvent * event )
{
	// QListView::dropEvent in short:
	// 1. if mode is InternalMove then discard events from external sources and copy actions
	// 2. get drop index from cursor position
	// 3. if model->dropMimeData then accept drop event
	superClass::dropEvent( event );

	// announce dropped files now only if it's an external drag&drop
	// otherwise postpone it because of the issue decribed at the top
	if (isExternFileDnD( event ))
		itemsDropped();
}

void EditableListView::startDrag( Qt::DropActions supportedActions )
{
	superClass::startDrag( supportedActions );

	// at this point the drag&drop should be finished and source rows removed, so we can safely update the selection
	itemsDropped();
}

void EditableListView::itemsDropped()
{
	// idiotic workaround because Qt is fucking retarded   (read the comment at the top)
	//
	// retrieve the destination drop indexes from the model and update the selection accordingly

	if (DropTargetListModel * model = dynamic_cast< DropTargetListModel * >( this->model() ))
	{
		if (model->wasDroppedInto())
		{
			int row = model->droppedRow();
			int count = model->droppedCount();

			deselectSelectedItems( this );
			for (int i = 0; i < count; i++)
				selectItemByIdx( this, row + i );

			emit itemsDropped( row, count );

			model->resetDropState();
		}
	}
	else
	{
		qWarning() << "EditableListView should be used only together with EditableListModel, "
		              "otherwise drag&drop will not work properly.";
	}
}

void EditableListView::toggleNameEditing( bool enabled )
{
	allowEditNames = enabled;

	if (enabled)
		setEditTriggers( QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed );
	else
		setEditTriggers( QAbstractItemView::NoEditTriggers );
}
