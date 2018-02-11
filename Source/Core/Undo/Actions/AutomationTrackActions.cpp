/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "AutomationTrackActions.h"
#include "MidiTrackSource.h"
#include "AutomationTrackTreeItem.h"
#include "SerializationKeys.h"

//===----------------------------------------------------------------------===//
// Insert
//===----------------------------------------------------------------------===//

AutomationTrackInsertAction::AutomationTrackInsertAction(MidiTrackSource &source,
    WeakReference<TreeItem> parentTreeItem) :
    UndoAction(source), parentTreeItem(parentTreeItem) {}

AutomationTrackInsertAction::AutomationTrackInsertAction(MidiTrackSource &source,
    WeakReference<TreeItem> parentTreeItem,
    String targetSerializedState, String targetXPath) :
    UndoAction(source),
    parentTreeItem(parentTreeItem),
    serializedState(std::move(targetSerializedState)),
    trackName(std::move(targetXPath)) {}

bool AutomationTrackInsertAction::perform()
{
    MidiTrackTreeItem *track = new AutomationTrackTreeItem("empty");
    this->parentTreeItem->addChildTreeItem(track);
    
    ScopedPointer<XmlElement> layerState = XmlDocument::parse(this->serializedState);
    track->deserialize(*layerState);
    
    this->trackId = track->getTrackId().toString();
    track->setTrackName(this->trackName, true);
    
    return true;
}

bool AutomationTrackInsertAction::undo()
{
    if (AutomationTrackTreeItem *treeItem =
        this->source.findTrackById<AutomationTrackTreeItem>(this->trackId))
    {
        // here the item state should be the same as when it was created
        // so don't serialize anything again
        return this->parentTreeItem->deleteItem(treeItem);
    }
    
    return false;
}

int AutomationTrackInsertAction::getSizeInUnits()
{
    return this->trackName.length();
}

ValueTree AutomationTrackInsertAction::serialize() const
{
    ValueTree tree(Serialization::Undo::automationTrackInsertAction);
    tree.setProperty(Serialization::Undo::xPath, this->trackName);
    tree.setProperty(Serialization::Undo::trackId, this->trackId);
    tree.appendChild(XmlDocument::parse(this->serializedState));
    return tree;
}

void AutomationTrackInsertAction::deserialize(const ValueTree &tree)
{
    this->trackName = tree.getProperty(Serialization::Undo::xPath);
    this->trackId = tree.getProperty(Serialization::Undo::trackId);
    this->serializedState = tree.getFirstChildElement()->createDocument("");
}

void AutomationTrackInsertAction::reset()
{
    this->trackName.clear();
    this->trackId.clear();
    this->serializedState.clear();
}

//===----------------------------------------------------------------------===//
// Remove
//===----------------------------------------------------------------------===//

AutomationTrackRemoveAction::AutomationTrackRemoveAction(MidiTrackSource &source,
    WeakReference<TreeItem> parentTreeItem) :
    UndoAction(source), parentTreeItem(parentTreeItem) {}

AutomationTrackRemoveAction::AutomationTrackRemoveAction(MidiTrackSource &source,
    WeakReference<TreeItem> parentTreeItem, String targetLayerId) :
    UndoAction(source),
    parentTreeItem(parentTreeItem),
    trackId(std::move(targetLayerId)),
    numEvents(0) {}

bool AutomationTrackRemoveAction::perform()
{
    if (AutomationTrackTreeItem *treeItem =
        this->source.findTrackById<AutomationTrackTreeItem>(this->trackId))
    {
        this->numEvents = treeItem->getSequence()->size();
        this->serializedTreeItem = treeItem->serialize();
        this->trackName = treeItem->getTrackName();
        return this->parentTreeItem->deleteItem(treeItem);
    }
    
    return false;
}

bool AutomationTrackRemoveAction::undo()
{
    if (this->serializedTreeItem != nullptr)
    {
        MidiTrackTreeItem *track = new AutomationTrackTreeItem("empty");
        this->parentTreeItem->addChildTreeItem(track);
        track->deserialize(*this->serializedTreeItem);
        track->setTrackName(this->trackName, true);
        return true;
    }
    
    return false;
}

int AutomationTrackRemoveAction::getSizeInUnits()
{
    if (this->serializedTreeItem != nullptr)
    {
        return (this->numEvents * sizeof(MidiEvent));
    }
    
    return 1;
}

ValueTree AutomationTrackRemoveAction::serialize() const
{
    ValueTree tree(Serialization::Undo::automationTrackRemoveAction);
    tree.setProperty(Serialization::Undo::xPath, this->trackName);
    tree.setProperty(Serialization::Undo::trackId, this->trackId);
    tree.appendChild(new XmlElement(*this->serializedTreeItem)); // deep copy
    return tree;
}

void AutomationTrackRemoveAction::deserialize(const ValueTree &tree)
{
    this->trackName = tree.getProperty(Serialization::Undo::xPath);
    this->trackId = tree.getProperty(Serialization::Undo::trackId);
    this->serializedTreeItem = new XmlElement(tree.getChild(0)); // deep copy
}

void AutomationTrackRemoveAction::reset()
{
    this->trackName.clear();
    this->trackId.clear();
    this->serializedTreeItem->deleteAllChildElements();
}
