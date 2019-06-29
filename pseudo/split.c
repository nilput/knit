//documented structs in the api:
struct kstr_str {
    char *str; //null terminated
    int len;
    int cap;
};
struct kstr_list {
    char struct kstr_str *items;
    int len;
    int cap;
};

//
//program 1
int main(int argc, const char **argv) {
    struct kstr kstr;
    kstrx_init(&kstr);
    kstrx_set_str(&kstr, "name","john"); //short for kstrx_init(&kstr, ...)
    kstrx_eval(&kstr, "result = 'hello {name}!';");
    char *result = kstr_get_str(&kstr, "result");
    kstrx_deinit(&kstr);
}
//with macro sugar
int main(int argc, const char **argv) {
    kstr_start(); //macro, creates a local struct named kstr,
    kstr_set_str("name","john"); //short for kstrx_init(&kstr, ...)
    kstr_eval("result = 'hello {name}!';");
    char *result = kstr_get_str("result");
    kstr_end(); //macro, destroys local struct
}

//program 2
int main(int argc, const char **argv) {
    struct kstr kstr;
    kstrx_init(&kstr);
    const char *input_colors = "black,white,red,green,";
    const char *input_email = "  foo@gmail.com ";

    kstrx_set_str(&kstr, "colors", input_colors); 
    kstrx_set_str(&kstr, "email", input_email); 
    kstrx_eval(&kstr, "clist = colors.split(',');"
                      "email = email.strip();"
                      "email_part = email[0:email.find('@')];");
    char *email_part = kstrx_get_str(&kstr, "email_part");
    struct kstr_list *list = kstrx_get_list(&kstr, "clist");
    for (int i=0; i<list->len; i++) {
        printf("color: %s\n", list->items[i]);
    }
    printf("email first part: %s\n", email_part);
    kstrx_deinit(&kstr); //frees all memory
}
//with macro sugar
int main(int argc, const char **argv) {
    kstr_start(); //macro, creates a local struct named kstr,
    const char *input_colors = "black,white,red,green,";
    const char *input_email = "  foo@gmail.com ";

    kstr_set_str("colors", input_colors); 
    kstr_set_str("email", input_email); 
    kstr_eval("clist = colors.split(',');"
              "email = email.strip();"
              "email_part = email[0:email.find('@')];");
    char *email_part = kstr_get_str("email_part");
    struct kstr_list *list = kstr_get_list("clist");
    for (int i=0; i<list->len; i++) {
        printf("color: %s\n", list->items[i].str);
    }
    printf("email first part: %s\n", email_part);
    kstr_end(); //frees all memory
}
