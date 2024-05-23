import re
from collections import Counter

# Open the file in read mode with 'utf-8' encoding and read the entire text
with open('pg2600.txt', 'r', encoding='utf-8') as file:
    text = file.read()

# Initialize counters and sets
word_count, vowel_count, sentence_count, paragraph_count = 0, 0, 0, 0
distinct_words = set()
letter_freq, word_freq, sentence_freq, pair_word_freq = Counter(), Counter(), Counter(), Counter()
vowels = 'aeiouAEIOU'

# Split the text into paragraphs
paragraphs = text.split('\n\n')
paragraph_count = len(paragraphs)

for paragraph in paragraphs:
    # Split the paragraph into sentences
    sentences = re.split(r'(?<=[.!?])\s+', paragraph.strip())

    for sentence in sentences:
        # Remove trailing quotation marks
        sentence = sentence.rstrip('”')

        # Check if the sentence is a chapter title
        if not sentence.startswith('CHAPTER'):
            # Update the sentence frequency before splitting the sentence into words
            sentence_freq[sentence] += 1
            sentence_count += 1

            words = sentence.split()
            word_count += len(words)
            distinct_words.update(words)
            vowel_count += sum(1 for char in sentence if char in vowels)
            letter_freq.update(char.lower() for char in sentence if char.isalpha())
            word_freq.update(words)
            pair_word_freq.update(zip(words, words[1:]))

# Print the results
print(f"The text contains {word_count} words, {vowel_count} vowels, {len(distinct_words)} distinct words, {sentence_count} sentences, and {paragraph_count} paragraphs.")
print(f"Top 5 letter frequencies: {letter_freq.most_common(5)}")
print(f"Top 5 word frequencies: {word_freq.most_common(5)}")
print("Top 5 sentence frequencies:")
for sentence, freq in sentence_freq.most_common(5):
    print(f"'{sentence}': {freq}")
print(f"Top 5 pair of word frequencies: {pair_word_freq.most_common(5)}")
