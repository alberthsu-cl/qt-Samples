/****************************************************************************
** Generated QML type registration code
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <QtQml/qqml.h>
#include <QtQml/qqmlmoduleregistration.h>

#if __has_include(<EditorViewModel.h>)
#  include <EditorViewModel.h>
#endif
#if __has_include(<MediaLibraryModel.h>)
#  include <MediaLibraryModel.h>
#endif


#if !defined(QT_STATIC)
#define Q_QMLTYPE_EXPORT Q_DECL_EXPORT
#else
#define Q_QMLTYPE_EXPORT
#endif
Q_QMLTYPE_EXPORT void qml_register_types_QmlMiniEditor()
{
    QT_WARNING_PUSH QT_WARNING_DISABLE_DEPRECATED
    qmlRegisterTypesAndRevisions<EditorViewModel>("QmlMiniEditor", 1);
    qmlRegisterTypesAndRevisions<MediaLibraryModel>("QmlMiniEditor", 1);
    qmlRegisterEnum<MediaLibraryModel::Role>("MediaLibraryModel::Role");
    QMetaType::fromType<QAbstractItemModel *>().id();
    qmlRegisterEnum<QAbstractItemModel::LayoutChangeHint>("QAbstractItemModel::LayoutChangeHint");
    qmlRegisterEnum<QAbstractItemModel::CheckIndexOption>("QAbstractItemModel::CheckIndexOption");
    QMetaType::fromType<QAbstractListModel *>().id();
    QT_WARNING_POP
    qmlRegisterModule("QmlMiniEditor", 1, 0);
}

static const QQmlModuleRegistration qmlMiniEditorRegistration("QmlMiniEditor", qml_register_types_QmlMiniEditor);
