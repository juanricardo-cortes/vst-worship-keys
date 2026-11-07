#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <memory>

// Android: the samples ship inside the APK under assets/<prefix>/... There are no real files to read there, so on
// the first launch (and after each app update) they are unpacked into the app's data folder - the same
// commonApplicationDataDirectory/<prefix> folder the sample search already looks in.
#if JUCE_ANDROID
inline bool unpackAndroidAssets (const juce::String& prefix, const std::function<void (const juce::String&)>& status)
{
    auto apk = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    auto destRoot = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile (prefix);
    auto marker = destRoot.getChildFile (".unpacked");
    auto stamp = juce::String (apk.getSize()) + "-" + juce::String (apk.getLastModificationTime().toMilliseconds());
    if (marker.existsAsFile() && marker.loadFileAsString() == stamp)
        return true;

    juce::ZipFile zip (apk);
    auto assetPrefix = "assets/" + prefix + "/";
    juce::Array<int> entries;
    for (int i = 0; i < zip.getNumEntries(); ++i)
        if (auto* e = zip.getEntry (i); e->filename.startsWith (assetPrefix) && ! e->filename.endsWithChar ('/'))
            entries.add (i);
    if (entries.isEmpty())
        return false;

    destRoot.deleteRecursively();
    destRoot.createDirectory();
    for (int n = 0; n < entries.size(); ++n)
    {
        if (n % 10 == 0)
            status ("Unpacking samples (first launch only)... " + juce::String (n * 100 / entries.size()) + "%");
        auto i = entries[n];
        auto target = destRoot.getChildFile (zip.getEntry (i)->filename.substring (assetPrefix.length()));
        target.getParentDirectory().createDirectory();
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
        juce::FileOutputStream out (target);
        if (in == nullptr || ! out.openedOk())
            return false;
        out.setPosition (0);
        out.truncate();
        out.writeFromInputStream (*in, -1);
    }
    return marker.replaceWithText (stamp);
}
#endif
