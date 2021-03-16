function effectiveNavDirection(flow, layoutDirection, direction) {
    if (direction === Qt.LeftArrow) {
        if (flow === GridView.FlowLeftToRight) {
            if (layoutDirection === Qt.LeftToRight) {
                return Qt.LeftArrow;
            } else {
                return Qt.RightArrow;
            }
        } else {
            if (layoutDirection === Qt.LeftToRight) {
                return Qt.UpArrow;
            } else {
                return Qt.DownArrow;
            }
        }
    } else if (direction === Qt.RightArrow) {
        if (flow === GridView.FlowLeftToRight) {
            if (layoutDirection === Qt.LeftToRight) {
                return Qt.RightArrow;
            } else {
                return Qt.LeftArrow;
            }
        } else {
            if (layoutDirection === Qt.LeftToRight) {
                return Qt.DownArrow;
            } else {
                return Qt.UpArrow;
            }
        }
    } else if (direction === Qt.UpArrow) {
        if (flow === GridView.FlowLeftToRight) {
            return Qt.UpArrow;
        } else {
            return Qt.LeftArrow;
        }
    } else if (direction === Qt.DownArrow) {
        if (flow === GridView.FlowLeftToRight) {
            return Qt.DownArrow;
        } else {
            return Qt.RightArrow
        }
    }
}
