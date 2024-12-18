
#ifndef POSITIONER_H
#define POSITIONER_H

#include "Litha.h"
#include <vector>

class Positioner
{
protected:
    video::IVideoDriver *driver;

    std::vector<gui::IGUIElement *> elements;

    s32 spacing;

    void SetTopLeft(gui::IGUIElement *element, s32 x, s32 y);
    void SetTopRight(gui::IGUIElement *element, s32 x, s32 y);

public:
    Positioner(video::IVideoDriver *driver, s32 spacing);
    virtual ~Positioner() {}

    void Add(gui::IGUIElement *element, s32 id);
    void SetSpacing(s32 spacing) { this->spacing = spacing; }

    virtual void Reset();
    virtual void Apply() = 0;

    // returns an id of the first element which the mouse is over
    // (we assume that elements will not intersect!)
    // -1 if none.
    s32 GetMouseOverId();

    // or NULL
    gui::IGUIElement *GetMouseOverElement();

    // This disabled as it's possible multiple buttons will share an ID.
    // gui::IGUIElement *GetElementById(s32 id);

    const std::vector<gui::IGUIElement *> &GetElements();
};

class RowPositioner : public Positioner
{
    s32 yPos;
    bool vertCentred;
    gui::IGUIElement *title;

public:
    // if not vertCentred, yPos is the top
    RowPositioner(video::IVideoDriver *driver, s32 yPos, s32 spacing,
                  bool vertCentred = true);
    virtual ~RowPositioner() {}

    // a "title" element that appears above the row...
    void SetTitle(gui::IGUIElement *element);
    void SetYPos(s32 yPos);

    void Apply() override;
    void Reset() override;
};

class ColumnPositioner : public Positioner
{
public:
    ColumnPositioner(video::IVideoDriver *driver, s32 spacing);

    void Apply() override;
};

class ColumnPositionerCentred : public Positioner
{
    f32 marginBottom;

public:
    ColumnPositionerCentred(video::IVideoDriver *driver, s32 spacing,
                            f32 marginBottom = 0.1);

    void Apply() override;
};

#endif
