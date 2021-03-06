#ifndef DIRECTIONPROFILEFRAME_H
#define DIRECTIONPROFILEFRAME_H

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/radiobutton.h>

#include "../../strategy/actions/directionprofileaction.h"

#include "../editstrategywindow.h"

class DirectionProfileFrame : public Gtk::Frame {
	public:
		DirectionProfileFrame(rfiStrategy::DirectionProfileAction &action, EditStrategyWindow &editStrategyWindow)
		: Gtk::Frame("Set flags"),
		_editStrategyWindow(editStrategyWindow), _action(action),
		_directionLabel("Direction: (function varies in that direction)"),
		_horizontalDirectionButton("Horizontal"),
		_verticalDirectionButton("Vertical"),
		_actionLabel("Action to perform:"),
		_storeModeButton("Store"),
		_applyModeButton("Apply"),
		_unapplyModeButton("Unapply"),
		_applyButton("Apply")
		{
			_box.pack_start(_directionLabel);
			
			Gtk::RadioButton::Group axisGroup;

			_box.pack_start(_horizontalDirectionButton);
			_horizontalDirectionButton.set_group(axisGroup);

			_box.pack_start(_verticalDirectionButton);
			_verticalDirectionButton.set_group(axisGroup);

			switch(_action.Axis())
			{
				default:
				case rfiStrategy::DirectionProfileAction::HorizontalAxis:
					_horizontalDirectionButton.set_active(true);
					break;
				case rfiStrategy::DirectionProfileAction::VerticalAxis:
					_verticalDirectionButton.set_active(true);
					break;
			}

			_box.pack_start(_actionLabel);

			Gtk::RadioButton::Group modeGroup;

			_box.pack_start(_storeModeButton);
			_storeModeButton.set_group(modeGroup);

			_box.pack_start(_applyModeButton);
			_applyModeButton.set_group(modeGroup);

			_box.pack_start(_unapplyModeButton);
			_unapplyModeButton.set_group(modeGroup);

			switch(action.ProfileAction())
			{
				case rfiStrategy::DirectionProfileAction::Store:
					_storeModeButton.set_active(true);
					break;
				case rfiStrategy::DirectionProfileAction::Apply:
					_applyModeButton.set_active(true);
					break;
				case rfiStrategy::DirectionProfileAction::Unapply:
					_unapplyModeButton.set_active(true);
					break;
			}
			
			_buttonBox.pack_start(_applyButton);
			_applyButton.signal_clicked().connect(sigc::mem_fun(*this, &DirectionProfileFrame::onApplyClicked));

			_box.pack_start(_buttonBox);

			add(_box);
			_box.show_all();
		}
	private:
		EditStrategyWindow &_editStrategyWindow;
		rfiStrategy::DirectionProfileAction &_action;

		Gtk::VBox _box;
		Gtk::ButtonBox _buttonBox;
		Gtk::Label _directionLabel;
		Gtk::RadioButton
			_horizontalDirectionButton,
			_verticalDirectionButton;
		Gtk::Label _actionLabel;
		Gtk::RadioButton
			_storeModeButton,
			_applyModeButton,
			_unapplyModeButton;
			
		Gtk::Button _applyButton;

		void onApplyClicked()
		{
			if(_horizontalDirectionButton.get_active())
				_action.SetAxis(rfiStrategy::DirectionProfileAction::HorizontalAxis);
			else if(_verticalDirectionButton.get_active())
				_action.SetAxis(rfiStrategy::DirectionProfileAction::VerticalAxis);
			
			if(_storeModeButton.get_active())
				_action.SetProfileAction(rfiStrategy::DirectionProfileAction::Store);
			else if(_applyModeButton.get_active())
				_action.SetProfileAction(rfiStrategy::DirectionProfileAction::Apply);
			else if(_unapplyModeButton.get_active())	
				_action.SetProfileAction(rfiStrategy::DirectionProfileAction::Unapply);
			
			_editStrategyWindow.UpdateAction(&_action);
		}
};

#endif // SETFLAGGINGFRAME_H
