#pragma once

#include "../../models/localDriver.hpp"

class SelfContained : public LocalDriver {

public:
  SelfContained();

  string useProxy(const string &dest, const string &genre,
                  const string &baseUrl) override;

  void applyConfig(json config) override;

  // Insert the given DetailManga object into the database.
  // The id and updateTime will be updated.
  // The thumbnail, chapters, and latest will be reset.
  // The given object will be deleted after inserting.
  Manga *createManga(DetailsManga *manga);

  // Update the given DetailManga object.
  // The given object will be deleted after updating.
  Manga *editManga(DetailsManga *manga);

  // Delete the Manga with the given id.
  void deleteManga(string id);

  // Create a new chapter with the given title, isExtra.
  // The extraData is referring to the manga's id.
  // It will also update the manga's updateTime and latest.
  Chapters createChapter(string extraData, string title, bool isExtra);

  // Update the given chapters.
  // It will update the index, title, and isExtra.
  // It will check for the completeness of the given chapters and it will update
  // the database only if it contains all the existing chapters.
  Chapters editChapters(Chapters chapters);

  // Delete the chapter with the given id and title.
  // The image will also be deleted.
  void deleteChapter(string id, string extraData);

  // Update the thumbnail of the given manga's id.
  // Old thumbnail will be deleted.
  vector<string> uploadThumbnail(string id, const string &image);

  // Append a image to the given chapter.
  vector<string> uploadMangaImage(string id, string extraData,
                                  const string &image);

  // Append images to the given chapter.
  vector<string> uploadMangaImages(string id, string extraData,
                                   vector<string> images);

  // Change the order of the image.
  // It will check for the completeness of the given urls and it will update the
  // database only if the urls contains all the existing urls.
  vector<string> arrangeMangaImage(string id, string extraData,
                                   vector<string> newUrls);

  // Remove the given image from the given chapter
  void deleteMangaImage(string id, string extraData, string url);

private:
  Chapters getChapters(string extraData);

  string generateId();

  string generateChapterId();

  int generateIndex(string id);
};