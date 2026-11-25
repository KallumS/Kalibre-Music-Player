#include "Plugins.h"

namespace kp
{

Plugins::Plugins (const juce::File& f) : folder (f)
{
    juce::addDefaultFormatsToManager (formats);
    if (auto xml = juce::XmlDocument::parse (knownFile())) known.recreateFromXml (*xml);
}

void Plugins::saveKnown()
{
    folder.createDirectory();
    if (auto xml = known.createXml()) xml->writeTo (knownFile());
}

juce::Array<juce::PluginDescription> Plugins::effects() const
{
    juce::Array<juce::PluginDescription> out;
    for (auto& d : known.getTypes()) if (! d.isInstrument) out.add (d);
    std::sort (out.begin(), out.end(), [] (auto& a, auto& b)
    {
        if (a.manufacturerName != b.manufacturerName) return a.manufacturerName.compareIgnoreCase (b.manufacturerName) < 0;
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    return out;
}

void Plugins::capture (Player& player)
{
    auto* c = player.chain();
    if (! c) return;
    for (size_t k = 0; k < c->plugins.size() && k < slots.size(); ++k)
    {
        juce::MemoryBlock state;
        c->plugins[k]->getStateInformation (state);
        slots[k].state = state;
    }
}

juce::String Plugins::apply (Player& player)
{
    const double rate = player.sampleRate();
    const int block = player.blockSize();
    auto chain = std::make_unique<PluginChain>();
    juce::StringArray failed;
    for (auto& s : slots)
    {
        juce::String error;
        auto p = formats.createPluginInstance (s.description, rate, block, error);
        if (! p) { failed.add (s.description.name); continue; }
        // Stereo in and out where it can.
        auto layout = p->getBusesLayout();
        if (layout.inputBuses.size() > 0) layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        if (layout.outputBuses.size() > 0) layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        p->setBusesLayout (layout);
        if (! s.state.isEmpty()) p->setStateInformation (s.state.getData(), (int) s.state.getSize());
        p->setRateAndBufferSizeDetails (rate, block);
        p->prepareToPlay (rate, block);
        chain->plugins.push_back (std::move (p));
        chain->bypassed.push_back (s.bypassed);
    }
    player.setChain (std::move (chain));
    return failed.isEmpty() ? juce::String() : failed.joinIntoString (", ") + (failed.size() == 1 ? " didn't" : " didn't") + " open.";
}

std::unique_ptr<juce::XmlElement> Plugins::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Chain");
    for (auto& s : slots)
    {
        auto* e = xml->createNewChildElement ("Slot");
        e->addChildElement (s.description.createXml().release());
        e->setAttribute ("state", s.state.toBase64Encoding());
        e->setAttribute ("bypassed", s.bypassed);
    }
    return xml;
}

void Plugins::fromXml (const juce::XmlElement& xml)
{
    slots.clear();
    for (auto* e : xml.getChildWithTagNameIterator ("Slot"))
    {
        Slot s;
        if (auto* d = e->getChildByName ("PLUGIN")) s.description.loadFromXml (*d);
        else continue;
        s.state.fromBase64Encoding (e->getStringAttribute ("state"));
        s.bypassed = e->getBoolAttribute ("bypassed");
        slots.push_back (std::move (s));
    }
}

} // namespace kp
