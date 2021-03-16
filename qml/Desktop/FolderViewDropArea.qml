import QtQuick 2.12
import MeuiKit 1.0 as Meui
import org.kde.draganddrop 2.0 as DragDrop

DragDrop.DropArea {
    id: dropArea

    property Item folderView: null

    function handleDragMove(folderView, pos) {
        // Trigger autoscroll.
        folderView.scrollLeft = (pos.x < (Meui.Units.largeSpacing * 3));
        folderView.scrollRight = (pos.x > width - (Meui.Units.largeSpacing * 3));
        folderView.scrollUp = (pos.y < (Meui.Units.largeSpacing * 3));
        folderView.scrollDown = (pos.y > height - (Meui.Units.largeSpacing * 3));

        folderView.handleDragMove(pos.x, pos.y);
    }

    function handleDragEnd(folderView) {
        // Cancel autoscroll.
        folderView.scrollLeft = false;
        folderView.scrollRight = false;
        folderView.scrollUp = false;
        folderView.scrollDown = false;

        folderView.endDragMove();
    }

    onDragMove: {
        // TODO: We should reject drag moves onto file items that don't accept drops
        // (cf. QAbstractItemModel::flags() here, but DeclarativeDropArea currently
        // is currently incapable of rejecting drag events.

        if (folderView) {
            handleDragMove(folderView, mapToItem(folderView, event.x, event.y));
        }
    }

    onDragLeave: {
        if (folderView) {
            handleDragEnd(folderView);
        }
    }

    onDrop: {
        if (folderView) {
            handleDragEnd(folderView);

            folderView.drop(folderView, event, mapToItem(folderView, event.x, event.y));
        }
    }
}
