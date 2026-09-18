#ifndef ULTIMATUM_WEB_JOURNAL_H
#define ULTIMATUM_WEB_JOURNAL_H
#include "topicjournal.h"
class Person;
void webJournalLoad();
bool webJournalSave(const std::string &directory);
void webJournalObserve(const TopicJournal::Passage &passage);
void webJournalDialogue(const std::string &text, Person *talker, const std::string &topic = "");
bool webJournalIsOpen();
#endif
