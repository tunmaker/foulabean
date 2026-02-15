# QML ↔ C++ Best Practices for Performance & Reactivity (Qt 6.10.1+)

> Minimum target: **Qt 6.10.1**. All recommendations use Qt 6 APIs exclusively. Legacy Qt 5 patterns like `qmlRegisterType` and `setContextProperty` are not covered.

---

## 1. Expose C++ Types with `QML_ELEMENT`

Use the declarative registration macros. They integrate with the QML compiler, enable type-checking at compile time, and produce faster code than runtime registration.

```cpp
// backend.h
#include <QObject>
#include <QtQml/qqmlregistration.h>

class Backend : public QObject {
    Q_OBJECT
    QML_ELEMENT  // makes this type available in QML as "Backend"

    Q_PROPERTY(QString status READ status WRITE setStatus NOTIFY statusChanged FINAL)

public:
    explicit Backend(QObject *parent = nullptr);
    QString status() const { return m_status; }
    void setStatus(const QString &s) {
        if (m_status == s) return;
        m_status = s;
        emit statusChanged();
    }

signals:
    void statusChanged();

private:
    QString m_status;
};
```

In CMake, declare the module with `qt_add_qml_module`:

```cmake
qt_add_qml_module(myapp
    URI MyApp
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES backend.h backend.cpp
)
```

For singletons, use `QML_SINGLETON` instead. For uncreatable types exposed only for enums, use `QML_UNCREATABLE`.

---

## 2. Property System — Guard Every Setter

Every `Q_PROPERTY` with a `NOTIFY` signal must guard against redundant emissions. Unguarded setters cause cascading binding re-evaluations in QML even when nothing changed.

```cpp
void setValue(int v) {
    if (m_value == v) return;  // always guard
    m_value = v;
    emit valueChanged();
}
```

Mark properties `FINAL` when they won't be overridden in QML. This allows the QML compiler to optimize access.

Keep properties **granular**: expose `firstName` and `lastName` separately rather than a monolithic `user` object. QML only re-evaluates bindings that depend on the specific property that changed.

---

## 3. Models — Use `QRangeModel` for Simple Cases, `QAbstractListModel` for Complex Ones

### QRangeModel (Qt 6.10+) — First Choice for Flat Data

`QRangeModel` wraps standard C++ containers (`std::vector`, `std::array`, any iterable) and exposes them directly to QML views without writing a full model subclass. It auto-generates roles from `Q_GADGET` properties.

```cpp
struct Contact {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString email MEMBER email)
public:
    QString name;
    QString email;
};

// Expose to QML
std::vector<Contact> contacts = { {"Alice", "a@x.com"}, {"Bob", "b@x.com"} };
auto *model = new QRangeModel(contacts);
```

```qml
ListView {
    model: contactModel
    delegate: Text { text: name + " — " + email }
}
```

### QAbstractListModel — When You Need Full Control

Use `QAbstractListModel` when you need dynamic insertion/removal with granular notifications, custom data transformation, or lazy loading. Always use `beginInsertRows`/`endInsertRows`, `beginRemoveRows`/`endRemoveRows`, and emit `dataChanged` with specific roles:

```cpp
QModelIndex idx = index(row);
emit dataChanged(idx, idx, {StatusRole});  // only this role, only this row
```

### TreeModel (QML, Qt 6.10+) — For Hierarchical Data in QML

For small hierarchical data sets or prototyping, declare tree structures directly in QML:

```qml
import QtQml.Models

TreeModel {
    id: orgChart
    TreeElement {
        property string name: "CEO"
        TreeElement { property string name: "CTO" }
        TreeElement { property string name: "CFO" }
    }
}

TreeView {
    model: orgChart
}
```

For large or dynamic trees from C++, use `QRangeModel` with nested structures or subclass `QAbstractItemModel`.

---

## 4. Sorting & Filtering — Use QML `SortFilterProxyModel`

Qt 6.10 introduces a declarative `SortFilterProxyModel` in QML. Use it instead of writing C++ `QSortFilterProxyModel` subclasses when the logic can be expressed declaratively.

```qml
import QtQml.Models

SortFilterProxyModel {
    id: filteredModel
    model: myBackendModel

    sorters: [
        RoleSorter { roleName: "priority"; sortOrder: Qt.DescendingOrder }
    ]

    filters: [
        FunctionFilter {
            component RoleData: QtObject { property bool active }
            function filter(data: RoleData): bool {
                return data.active
            }
        }
    ]
}
```

For performance-critical or complex filtering, keep the logic in a C++ `QSortFilterProxyModel` subclass. For everything else, the QML version reduces boilerplate and keeps filter/sort logic close to the UI.

> **Note:** `SortFilterProxyModel` is tech preview in Qt 6.10. API may evolve in 6.11+.

---

## 5. Property Synchronization with `Synchronizer`

Qt 6.10 introduces the `Synchronizer` QML type to keep two or more properties in sync without manual binding chains. Use it when you need bidirectional sync between backend properties and UI state, replacing fragile manual `onXChanged` handlers that can create binding loops.

> **Note:** `Synchronizer` is tech preview in Qt 6.10.

---

## 6. Threading — Keep the UI Thread Free

The main thread renders QML. Any blocking work causes dropped frames.

**Offload heavy work** to `QtConcurrent::run` (preferred for one-off tasks) or `QThread` (for long-lived workers). Qt automatically uses `Qt::QueuedConnection` across threads.

```cpp
void Backend::fetchData() {
    QtConcurrent::run([this]() {
        auto result = expensiveComputation();
        // Safe: auto-queued to the main thread
        QMetaObject::invokeMethod(this, [this, result]() {
            setResult(result);  // triggers QML binding update
        });
    });
}
```

**Never touch QML objects from a worker thread.** Always emit signals or use `QMetaObject::invokeMethod` with `Qt::QueuedConnection` to push data to the main thread.

---

## 7. Minimize QML ↔ C++ Boundary Crossings

Every call across the boundary has overhead (meta-object lookup, variant conversion).

- **Batch related updates.** If updating 10 properties, set them all then emit signals, or group related data into a `Q_GADGET` struct exposed as a single property.
- **Avoid calling C++ methods per-frame from QML.** Do per-frame logic in C++ and push results via properties.
- **Prefer declarative bindings over imperative JS.** `text: backend.status` is faster than an `onStatusChanged` handler that sets `text` manually.
- **Use `required property` in delegates** instead of accessing roles via `model.roleName`. This enables the QML compiler to generate faster code.

```qml
delegate: ItemDelegate {
    required property string name
    required property int index
    text: name
}
```

---

## 8. Keep JavaScript in QML Minimal

QML's V4 JS engine is much slower than native C++.

- **Move non-trivial logic to C++.** Sorting, filtering, string manipulation, math — all belong in C++ exposed via properties or `Q_INVOKABLE`.
- **Keep signal handlers to 1–3 lines.** If it's more, move it to C++.
- **Avoid creating JS arrays/objects in tight loops.** This causes GC pressure and micro-stutters.
- **Use the QML compiler (`qmlsc` / `qmlcachegen`).** With `qt_add_qml_module` and properly typed QML, the compiler generates C++ from your QML.

---

## 9. Lazy Loading & Component Lifecycle

- **Use `Loader` with `asynchronous: true`** for heavy sub-components that aren't always visible.
- **Use `ListView` / `GridView`** instead of `Repeater` for collections larger than ~20 items. These views recycle delegates.
- **Set `cacheBuffer`** on list views to control how many off-screen delegates stay alive.
- **Use `FlexboxLayout` (Qt 6.10+)** for responsive arrangement of items. It follows the CSS Flexbox model and handles dynamic resizing without manual anchoring.

```qml
ListView {
    model: filteredModel
    delegate: MyDelegate {}
    cacheBuffer: 400
}
```

For complex delegates, use `DelegateModel` with `delegateModelAccess` (Qt 6.10+) to write through required properties into the model.

---

## 10. Image & Resource Handling

- **Always set `asynchronous: true`** on `Image` for non-trivial images.
- **Always set `sourceSize`** to avoid loading full-resolution images into GPU memory.
- **Use `QQuickImageProvider`** for dynamic/generated images from C++.

```qml
Image {
    source: "image://myprovider/photo123"
    asynchronous: true
    sourceSize: Qt.size(200, 200)
}
```

---

## 11. Connections & Signal Management

Use `Connections` with `enabled` to avoid processing signals when a component is off-screen:

```qml
Connections {
    target: backend
    enabled: visible
    function onDataChanged() { /* ... */ }
}
```

Avoid deep signal chains (A → B → C → D → QML). Flatten where possible.

---

## 12. Use `Q_INVOKABLE` for Actions, Not Data Flow

- `Q_INVOKABLE` is appropriate for user-triggered actions (button clicks, form submissions).
- **Do not use it for data flow.** Data should flow reactively via properties + bindings.
- If returning complex data, prefer setting a property + emitting a signal over returning a `QVariantMap`.

---

## 13. Build & Tooling

- **Enable the QML compiler** via `qt_add_qml_module` with typed QML. This compiles QML to C++ for significant runtime gains.
- **Use `DISCARD_QML_CONTENTS`** (Qt 6.10+) to strip QML/JS source from the binary when using compiled QML:

```cmake
qt_add_qml_module(myapp
    URI MyApp
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES backend.h backend.cpp
    DISCARD_QML_CONTENTS
)
```

- **Run `qmllint`** in CI. It catches type errors, unresolved properties, and performance anti-patterns.

---

## 14. Profiling

- **QML Profiler** (Qt Creator → Analyze): binding re-evaluations, signal handling time, component creation, and rendering.
- **`QSG_RENDER_TIMING=1`**: scene graph render times to stdout.
- **`QML_IMPORT_TRACE=1`**: import resolution, useful for startup slowness.

---

## Quick Reference: Anti-Patterns → Correct Patterns

| Don't | Do Instead |
|---|---|
| `qmlRegisterType` / `setContextProperty` | `QML_ELEMENT` + `qt_add_qml_module` |
| Expose `QList<QObject*>` for lists | `QRangeModel` (simple), `QAbstractListModel` (dynamic) |
| C++ `QSortFilterProxyModel` subclass for simple filtering | QML `SortFilterProxyModel` (Qt 6.10+) |
| Heavy JS logic in QML handlers | Move to C++, expose via properties |
| Emit notify without guard | `if (old == new) return;` in every setter |
| Block main thread with I/O or computation | `QtConcurrent::run` or `QThread` |
| `Repeater` with 50+ items | `ListView` with delegate recycling |
| Access model roles via `model.roleName` | `required property` in delegates |
| Return data via `Q_INVOKABLE` getters | Push data via properties + NOTIFY signals |
| Full-res images without `sourceSize` | Always set `sourceSize` + `asynchronous: true` |
| Manual anchor math for responsive layouts | `FlexboxLayout` (Qt 6.10+) |

---

## Recommended Reading

- [What's New in Qt 6.10](https://doc.qt.io/qt-6/whatsnew610.html)
- [QML and C++ Integration Overview](https://doc.qt.io/qt-6/qtqml-cppintegration-overview.html)
- [Exposing C++ Attributes to QML](https://doc.qt.io/qt-6/qtqml-cppintegration-exposecppattributes.html)
- [Qt Quick Performance Considerations](https://doc.qt.io/qt-6/qtquick-performance.html)
- [Best Practices for QML and Qt Quick](https://doc.qt.io/qt-6/qtquick-bestpractices.html)
- [SortFilterProxyModel QML Type](https://doc.qt.io/qt-6/qml-qtqml-models-sortfilterproxymodel.html)
- [SortFilterProxyModel Blog Post](https://www.qt.io/blog/model-to-sort-and-filter-the-data-on-the-fly)
- [Qt 6.10 Release Blog](https://www.qt.io/blog/qt-6.10-released)
- [Defining QML Types from C++](https://doc.qt.io/qt-6/qtqml-cppintegration-definetypes.html)
- [Threading Basics in Qt](https://doc.qt.io/qt-6/thread-basics.html)
