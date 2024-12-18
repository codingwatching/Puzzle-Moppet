
#include "SimpleMenu.h"
#include "Positioner.h"
#include "Colors.h"

// taken from mainstate
gui::IGUIStaticText *add_static_text2(const wchar_t *str);
gui::IGUIStaticText *add_static_text(const wchar_t *str);

SimpleMenu::SimpleMenu(s32 uniqueId)
{
    id = uniqueId;

    engine = GetEngine();
    device = engine->GetIrrlichtDevice();

    positioner = nullptr;

    // sound
    menuSound = engine->GetSoundSystem()->CreateSound2D();
    menuSound->SetVolume(0.5);

    engine->RegisterEventInterest(this, "ButtonDown");
    engine->RegisterEventInterest(this, "AxisMoved");
}

SimpleMenu::~SimpleMenu()
{
    if (positioner)
    {
        positioner->Reset();
        delete positioner;
    }

    menuSound->drop();

    engine->UnregisterAllEventInterest(this);
}

// In future, shutdown should probably also queue the menu for complete removal.
void SimpleMenu::Shutdown()
{
    // No more clicks or mouseovers.
    engine->UnregisterAllEventInterest(this);
}

void SimpleMenu::AddItem(core::stringw name, s32 uniqueItemId,
                         bool removeMenuOnClick)
{
    positioner->Add(add_static_text(name.c_str()), uniqueItemId);
}

void SimpleMenu::AddImageItem(io::path imageName, s32 uniqueItemId,
                              bool removeMenuOnClick)
{
    gui::IGUIEnvironment *guienv = device->getGUIEnvironment();
    video::IVideoDriver *driver = device->getVideoDriver();

    video::ITexture *texture = driver->getTexture(imageName);
    // core::dimension2du size = texture->getOriginalSize();

    gui::IGUIImage *guiimg = guienv->addImage(texture, core::position2di(0, 0));
    positioner->Add(guiimg, uniqueItemId);
}

void SimpleMenu::SetItemGap(u32 gap)
{
    positioner->SetSpacing(gap);
}

void SimpleMenu::Finalise()
{
    positioner->Apply();

    // Init the menu mouseover stuff.
    Event event("AxisMoved");
    event["axis"] = AXIS_MOUSE_X;
    event["SimpleMenu::Finalise"] = 1;
    engine->PostEvent(event);
}

void SimpleMenu::SetMouseOverSound(const core::stringc &sound)
{
    mouseOverSound = sound;
}

s32 SimpleMenu::GetId()
{
    return id;
}

const std::vector<gui::IGUIElement *> &SimpleMenu::GetElements()
{
    ASSERT(positioner);
    return positioner->GetElements();
}

void SimpleMenu::SetElementEnabled(s32 uniqueItemId, bool enabled)
{
    ASSERT(positioner);
    for (const auto &element : positioner->GetElements())
    {
        if (element->getID() == uniqueItemId)
        {
            element->setEnabled(enabled);

            // Update override color if text
            if (element->getType() == gui::EGUIET_STATIC_TEXT)
            {
                auto *textElement = static_cast<gui::IGUIStaticText *>(element);
                auto color =
                    enabled ? Colors::text_col() : Colors::text_col_disabled();
                textElement->setOverrideColor(color);
            }

            return;
        }
    }
    ASSERT(false && "element not found");
}

void SimpleMenu::OnEvent(const Event &event)
{
    if (event.IsType("ButtonDown"))
    {
        if (event["button"] == KEY_LBUTTON)
        {
            auto mouseOverElement = positioner->GetMouseOverElement();
            if (mouseOverElement && !mouseOverElement->isEnabled())
            {
                return;
            }

            s32 mouseOverId = positioner->GetMouseOverId();
            if (mouseOverId != -1)
            {
                Event event("MenuButton");
                event["menu"] = id;
                event["button"] = mouseOverId;
                engine->QueueEvent(event);

                // And shutdown the menu.
                Shutdown();
            }
        }
    }
    else if (event.IsType("AxisMoved"))
    {
        if (event["axis"] == AXIS_MOUSE_X || event["axis"] == AXIS_MOUSE_Y)
        {
            gui::IGUIElement *mouseOverElement =
                positioner->GetMouseOverElement();

            if (mouseOverElement && !mouseOverElement->isEnabled())
            {
                return;
            }

            // Update all colours

            const std::vector<gui::IGUIElement *> &elements =
                positioner->GetElements();

            for (auto &element : elements)
            {
                if (element->getType() == gui::EGUIET_STATIC_TEXT)
                {
                    auto *textElement = (gui::IGUIStaticText *)element;
                    // Hacky way to check if the element is disabled
                    bool isDisabled = textElement->getOverrideColor() ==
                                      Colors::text_col_disabled();

                    if (isDisabled)
                    {
                        // Do nothing
                    }
                    else if (element == mouseOverElement)
                    {
                        if (textElement->getOverrideColor() !=
                            Colors::text_col_mouseover())
                        {
                            if (mouseOverSound.size() &&
                                !event.HasKey("SimpleMenu::Finalise"))
                                menuSound->Play(mouseOverSound);
                        }

                        textElement->setOverrideColor(
                            Colors::text_col_mouseover());
                    }
                    else
                    {
                        textElement->setOverrideColor(Colors::text_col());
                    }
                }
                else if (element->getType() == gui::EGUIET_IMAGE)
                {
                    auto *imageElement = (gui::IGUIImage *)element;

                    if (element == mouseOverElement)
                    {
                        // Play sound if state not detected before, or if it was
                        // not previously mouse-overed.
                        if (!mouseOverStates.count(imageElement) ||
                            !mouseOverStates[imageElement])
                        {
                            if (mouseOverSound.size() &&
                                !event.HasKey("SimpleMenu::Finalise"))
                                menuSound->Play(mouseOverSound);
                        }

                        imageElement->setColor(
                            video::SColor(100, 255, 255, 255));
                        mouseOverStates[imageElement] = true;
                    }
                    else
                    {
                        imageElement->setColor(
                            video::SColor(255, 255, 255, 255));
                        mouseOverStates[imageElement] = false;
                    }
                }
            }
        }
    }
}

void SimpleMenu::Relayout()
{
    positioner->Apply();
}

// ***************** SimpleHorizontalMenu ****************

SimpleHorizontalMenu::SimpleHorizontalMenu(s32 uniqueId, s32 yPos, s32 spacing,
                                           bool vertCentred)
    : SimpleMenu(uniqueId)
{
    positioner =
        new RowPositioner(device->getVideoDriver(), yPos, spacing, vertCentred);
}

void SimpleHorizontalMenu::SetHeading(core::stringw text)
{
    ((RowPositioner *)positioner)->SetTitle(add_static_text(text.c_str()));
}

void SimpleHorizontalMenu::SetYPos(s32 yPos)
{
    ASSERT(dynamic_cast<RowPositioner *>(positioner) != nullptr);
    static_cast<RowPositioner *>(positioner)->SetYPos(yPos);
    positioner->Apply();
}

void SimpleHorizontalMenu::SetSpacing(s32 spacing)
{
    ASSERT(dynamic_cast<RowPositioner *>(positioner) != nullptr);
    static_cast<RowPositioner *>(positioner)->SetSpacing(spacing);
    positioner->Apply();
}

// ***************** SimpleVerticalMenu ****************

SimpleVerticalMenu::SimpleVerticalMenu(s32 uniqueId, f32 marginBottom)
    : SimpleMenu(uniqueId)
{
    // positioner = new ColumnPositioner(device->getVideoDriver(), 20);
    positioner =
        new ColumnPositionerCentred(device->getVideoDriver(), 10, marginBottom);
}
