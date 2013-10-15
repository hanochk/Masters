#include "layerdelete.h"

#include <model/layerlist.h>
#include <model/layer.h>

LayerDelete::LayerDelete(boost::shared_ptr<Layer> layer, LayerList * ll) {
    const std::set<uint16_t> & source_labels = layer->getLabelSet();
    std::copy(source_labels.begin(), source_labels.end(), std::back_inserter(labels_));
    col_ = layer->color_;
    name_ = layer->name_;
    ll_ = ll;
    layer_ = layer;
}

QString LayerDelete::actionText(){
    return "New Layer";
}

void LayerDelete::undo(){
    boost::shared_ptr<Layer> layer = ll_->addLayerWithId(id_);
    layer->setName(name_);
    layer->setColor(col_);
    for(uint16_t label : labels_){
        layer->addLabel(label);
    }
    layer_ = layer;
}

void LayerDelete::redo(){
    auto layer = layer_.lock();
    id_ = layer->id_;
    ll_->deleteLayer(layer);
}

bool LayerDelete::mergeWith(const QUndoCommand *other){
    return false;
}

int LayerDelete::id() const{
    return 5;
}
